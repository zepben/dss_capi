use std::assert_matches;

use super::*;
use crate::producer::{ConfirmationCallback, StreamProducer};
use async_trait::async_trait;
use rabbitmq_stream_client::{
    error::{ProducerCloseError, ProducerPublishError},
    types::{Message, ResponseCode},
};

use tokio::sync::{Mutex, MutexGuard};

struct TestProducerState {
    sent_messages: Vec<Vec<u8>>,
    /// The confirmation callback for the last message that was sent
    confirmation_callback: Option<ConfirmationCallback>,
    send_calls: usize,
    send_and_wait_confirmation_calls: usize,
    close_calls: usize,

    /// Results of `send` to simulate, in reverse order.
    send_results: Vec<Result<(), ProducerPublishError>>,
    /// Results of `send_and_wait_confirmation` to simulate, in reverse order.
    confirmation_results: Vec<ConfirmationResult>,
    close_result: Option<Result<(), ProducerCloseError>>,
    immediate_confirmation: Option<Confirmation>,
}

#[derive(Clone)]
struct TestProducer {
    state: Arc<Mutex<TestProducerState>>,
}

impl TestProducer {
    /// Confirm the last message sent with the given [`ConfirmationResult`].
    async fn confirm(&self, result: ConfirmationResult) {
        match self.state().await.confirmation_callback {
            ref mut callback @ Some(_) => callback.take().unwrap()(result).await,
            None => panic!("no confirmation callback registered"),
        }
    }

    async fn state(&self) -> MutexGuard<'_, TestProducerState> {
        self.state.lock().await
    }

    async fn set_confirmation_results<const N: usize>(&self, mut results: [ConfirmationResult; N]) {
        results.reverse();
        self.state().await.confirmation_results = Vec::from(results);
    }
}

#[async_trait]
impl StreamProducer for TestProducer {
    async fn send(
        &self,
        message: Message,
        on_confirmation: ConfirmationCallback,
    ) -> Result<(), ProducerPublishError> {
        self.state().await.send_calls += 1;

        self.state()
            .await
            .sent_messages
            .push(message.data().map_or(Vec::new(), |x| x.to_vec()));

        self.state().await.send_results.pop().unwrap_or(Ok(()))?; // propagate an error send result

        let immediate_confirmation = self.state().await.immediate_confirmation;
        match immediate_confirmation {
            Some(confirmation) => on_confirmation(Ok(confirmation)).await,
            None => self.state().await.confirmation_callback = Some(on_confirmation),
        }

        Ok(())
    }

    async fn send_and_wait_confirmation(&self, message: Message) -> ConfirmationResult {
        let mut state = self.state().await;
        state.send_and_wait_confirmation_calls += 1;
        state
            .sent_messages
            .push(message.data().map_or(Vec::new(), |x| x.to_vec()));
        state
            .confirmation_results
            .pop()
            .unwrap_or(Ok(Confirmation::Confirmed))
    }

    async fn close(self) -> Result<(), ProducerCloseError> {
        self.state().await.close_calls += 1;
        return self.state().await.close_result.take().unwrap();
    }
}

impl ResultsStream<TestProducer> {
    fn with<const N: usize>(
        mut send_results: [Result<(), ProducerPublishError>; N],
        close_result: Result<(), ProducerCloseError>,
        immediate_confirmation: Option<Confirmation>,
    ) -> ResultsStream<TestProducer> {
        let (messages_inflight_tx, inflight_messages) = watch::channel(0);

        send_results.reverse();

        ResultsStream {
            producer: TestProducer {
                state: Arc::new(Mutex::new(TestProducerState {
                    sent_messages: Vec::new(),
                    confirmation_callback: None,
                    send_calls: 0,
                    send_and_wait_confirmation_calls: 0,
                    close_calls: 0,
                    send_results: Vec::from(send_results),
                    confirmation_results: Vec::new(),
                    close_result: Some(close_result),
                    immediate_confirmation,
                })),
            },
            stats: Arc::new(Stats::new(messages_inflight_tx.clone())),
            inflight_messages,
        }
    }
}

#[tokio::test]
async fn successful_send_records_message_and_statistics() {
    let mut stream = ResultsStream::with([], Ok(()), None);

    stream.send(b"message body").await;

    assert_eq!(
        stream.producer.state.lock().await.sent_messages,
        vec![b"message body".to_vec()]
    );
    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.bytes_sent, 12);
    assert_eq!(stream.stats.messages_failed, 0);
}

#[tokio::test]
async fn record_messages_and_bytes_sent_for_successful_send() {
    let mut stream = ResultsStream::with([Ok(())], Ok(()), None);

    stream.send(b"message").await;

    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.bytes_sent, 7);
}

#[tokio::test]
async fn record_messages_and_bytes_sent_for_failed_send() {
    let mut stream = ResultsStream::with([Err(ProducerPublishError::Closed)], Ok(()), None);

    stream.send(b"not sent").await;

    assert_eq!(stream.producer.state.lock().await.send_calls, 1);
    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.bytes_sent, 8);
    assert_eq!(stream.stats.messages_failed, 1);
}

#[tokio::test(start_paused = true)]
async fn timeout_is_retried_and_success_is_counted_once() {
    let mut stream = ResultsStream::with(
        [
            Err(ProducerPublishError::Timeout),
            Err(ProducerPublishError::Timeout),
            Ok(()),
        ],
        Ok(()),
        None,
    );

    stream.send(b"eventually sent").await;

    assert_eq!(stream.producer.state.lock().await.send_calls, 3); // we tried to send 3 times
    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.bytes_sent, 15);
    assert_eq!(stream.stats.messages_failed, 0);
}

#[tokio::test(start_paused = true)]
async fn exhausted_timeout_retries_counts_one_failure() {
    let mut stream = ResultsStream::with(
        [
            Err(ProducerPublishError::Timeout),
            Err(ProducerPublishError::Timeout),
            Err(ProducerPublishError::Timeout),
            Err(ProducerPublishError::Timeout),
            Err(ProducerPublishError::Timeout),
        ],
        Ok(()),
        None,
    );

    stream.send(b"never sent").await;

    assert_eq!(stream.producer.state.lock().await.send_calls, 4);
    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.messages_failed, 1);
}

#[tokio::test]
async fn non_timeout_error_is_not_retried() {
    let mut stream = ResultsStream::with([Err(ProducerPublishError::Closed), Ok(())], Ok(()), None);

    stream.send(b"message").await;

    assert_eq!(stream.producer.state.lock().await.send_calls, 1);
    assert_eq!(stream.stats.messages_failed, 1);
}

#[tokio::test(start_paused = true)]
async fn synchronous_confirmation_timeout_is_retried() {
    let stream = ResultsStream::with([], Ok(()), None);
    stream
        .producer
        .set_confirmation_results([
            Err(ProducerPublishError::Timeout),
            Err(ProducerPublishError::Timeout),
            Ok(Confirmation::Confirmed),
        ])
        .await;

    stream
        .send_and_wait_confirmation(b"eventually confirmed")
        .await;

    assert_eq!(
        stream
            .producer
            .state()
            .await
            .send_and_wait_confirmation_calls,
        3
    );
    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.bytes_sent, 20);
    assert_eq!(stream.stats.messages_failed, 0);
}

#[tokio::test(start_paused = true)]
async fn synchronous_confirmation_does_not_leave_stale_state() {
    let mut stream = ResultsStream::with([], Ok(()), None);

    stream.send_and_wait_confirmation(b"message").await;

    let wait = stream.wait_no_inflight(Duration::from_secs(1)).await;
    assert_eq!(wait, Ok(()));
    assert_eq!(stream.stats.messages_confirmed, 1);
    assert_eq!(stream.stats.confirmation_wait_timeouts, 0);
}

#[tokio::test]
async fn immediate_confirmation_does_not_leave_stale_state() {
    let mut stream = ResultsStream::with([], Ok(()), Some(Confirmation::Confirmed));

    stream.send(b"message").await;

    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.messages_confirmed, 1);
    assert_eq!(stream.stats.bytes_sent, 7);
    assert_eq!(stream.stats.confirmation_wait_timeouts, 0);
}

#[tokio::test(start_paused = true)]
async fn wait_confirmation_times_out_if_not_all_confirmed() {
    let mut stream = ResultsStream::with([Ok(())], Ok(()), None);

    stream.send(b"message").await;

    let wait = stream.wait_no_inflight(Duration::from_secs(1)).await;
    assert_eq!(wait, Err(())); // we timed out waiting on confirmation
    assert_eq!(stream.stats.confirmation_wait_timeouts, 1);
}

#[tokio::test(start_paused = true)]
async fn wait_inflight_confirmed_message() {
    let mut stream = ResultsStream::with([Ok(())], Ok(()), None);

    stream.send(b"message").await;

    stream.producer.confirm(Ok(Confirmation::Confirmed)).await;
    let wait = stream.wait_no_inflight(Duration::from_secs(1)).await;

    assert_eq!(wait, Ok(()));
    assert_eq!(stream.stats.confirmation_wait_timeouts, 0);
}

#[tokio::test(start_paused = true)]
async fn wait_inflight_unconfirmed_message() {
    let mut stream = ResultsStream::with([Ok(())], Ok(()), None);

    stream.send(b"message").await;
    stream.producer.confirm(Ok(Confirmation::Unconfirmed)).await;

    let wait = stream.wait_no_inflight(Duration::from_secs(1)).await;
    assert_eq!(wait, Ok(()));
    assert_eq!(stream.stats.confirmation_wait_timeouts, 0);
    assert_eq!(stream.stats.messages_confirmed, 0);
    assert_eq!(stream.stats.messages_unconfirmed, 1);
}

#[tokio::test(start_paused = true)]
async fn wait_inflight_failed_message() {
    let mut stream = ResultsStream::with([Ok(())], Ok(()), None);

    stream.send(b"message").await;
    stream
        .producer
        .confirm(Err(ProducerPublishError::Closed))
        .await;

    let wait = stream.wait_no_inflight(Duration::from_secs(1)).await;
    assert_eq!(wait, Ok(()));
    assert_eq!(stream.stats.confirmation_wait_timeouts, 0);
    assert_eq!(stream.stats.messages_confirmed, 0);
    assert_eq!(stream.stats.messages_failed, 1);
}

#[tokio::test]
async fn wait_inflight_none_sent() {
    let mut stream = ResultsStream::with([], Ok(()), None);

    let result = tokio::time::timeout(
        Duration::from_millis(1),
        stream.wait_no_inflight(Duration::from_secs(1)),
    )
    .await;
    assert_matches!(result, Ok(_));
    assert_eq!(stream.stats.confirmation_wait_timeouts, 0);
}

#[tokio::test]
async fn publisher_does_not_exist_is_not_a_disconnect_failure() {
    let producer_does_not_exist = ProducerCloseError::Close {
        stream: "stream".into(),
        status: ResponseCode::PublisherDoesNotExist,
    };
    let stream = ResultsStream::with([], Err(producer_does_not_exist), None);
    let stats = stream.stats.clone(); // this is an Arc, so is still shared with the stream

    stream.disconnect().await;

    assert_eq!(stats.disconnects_failed, 0);
}

#[tokio::test]
async fn disconnect_error_is_recorded() {
    let stream = ResultsStream::with([], Err(ProducerCloseError::AlreadyClosed), None);
    let stats = stream.stats.clone(); // this is an Arc, so is still shared with the stream

    stream.disconnect().await;
    assert_eq!(stats.disconnects_failed, 1);
}
