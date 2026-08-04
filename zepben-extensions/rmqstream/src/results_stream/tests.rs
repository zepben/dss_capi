use super::*;
use crate::{
    producer::{ConfirmationCallback, StreamProducer},
    retry::MAX_RETRIES,
};
use async_trait::async_trait;
use rabbitmq_stream_client::{
    error::{ProducerCloseError, ProducerPublishError},
    types::{Message, ResponseCode},
};
use std::sync::Mutex;

#[derive(Clone, Copy)]
enum SendResult {
    Success,
    Timeout,
    Closed,
}

struct TestProducerState {
    sent_messages: Vec<Vec<u8>>,
    confirmations: Vec<Option<ConfirmationCallback>>,
    send_calls: usize,
    close_calls: usize,
}

impl Default for TestProducerState {
    fn default() -> Self {
        Self {
            sent_messages: Vec::new(),
            confirmations: Vec::new(),
            send_calls: 0,
            close_calls: 0,
        }
    }
}

#[derive(Clone)]
struct TestProducer {
    state: Arc<Mutex<TestProducerState>>,
    /// Results of `send` to simulate, in reverse order.
    send_results: Arc<Mutex<Vec<SendResult>>>,
    close_result: Arc<Mutex<Option<Result<(), ProducerCloseError>>>>,
    immediate_confirmation: Option<Confirmation>,
}

impl TestProducer {
    fn send_calls(&self) -> usize {
        self.state.lock().unwrap().send_calls
    }

    fn close_calls(&self) -> usize {
        self.state.lock().unwrap().close_calls
    }

    fn sent_messages(&self) -> Vec<Vec<u8>> {
        self.state.lock().unwrap().sent_messages.clone()
    }

    async fn confirm(&self, index: usize, result: ConfirmationResult) {
        let callback = self
            .state
            .lock()
            .unwrap()
            .confirmations
            .get_mut(index)
            .and_then(Option::take)
            .expect("confirmation callback was not available");
        callback(result).await;
    }
}

#[async_trait]
impl StreamProducer for TestProducer {
    async fn send(
        &self,
        message: &Message,
        on_confirmation: ConfirmationCallback,
    ) -> Result<(), ProducerPublishError> {
        self.state.lock().unwrap().send_calls += 1;
        self.state
            .lock()
            .unwrap()
            .sent_messages
            .push(message.data().map_or(Vec::new(), |x| x.to_vec()));
        let result = self
            .send_results
            .lock()
            .unwrap()
            .pop()
            .unwrap_or(SendResult::Success);
        let immediate_confirmation = self.immediate_confirmation;

        match result {
            SendResult::Success => {
                if let Some(confirmation) = immediate_confirmation {
                    on_confirmation(Ok(confirmation)).await;
                } else {
                    self.state
                        .lock()
                        .unwrap()
                        .confirmations
                        .push(Some(on_confirmation));
                }
                Ok(())
            }
            SendResult::Timeout => Err(ProducerPublishError::Timeout),
            SendResult::Closed => Err(ProducerPublishError::Closed),
        }
    }

    async fn close(self) -> Result<(), ProducerCloseError> {
        self.state.lock().unwrap().close_calls += 1;
        return self.close_result.lock().unwrap().take().unwrap();
    }
}

#[derive(Clone, Copy)]
struct PendingStateObservation {
    messages_sent: u64,
    all_messages_confirmed: bool,
}

struct PendingStateObservingProducer {
    confirmation_state: Receiver<bool>,
    stats: Arc<Stats>,
    observation: Arc<Mutex<Option<PendingStateObservation>>>,
}

#[async_trait]
impl StreamProducer for PendingStateObservingProducer {
    async fn send(
        &self,
        _message: &Message,
        _on_confirmation: ConfirmationCallback,
    ) -> Result<(), ProducerPublishError> {
        *self.observation.lock().unwrap() = Some(PendingStateObservation {
            messages_sent: self.stats.messages_sent.get(),
            all_messages_confirmed: *self.confirmation_state.borrow(),
        });
        Ok(())
    }

    async fn close(self) -> Result<(), ProducerCloseError> {
        Ok(())
    }
}

impl ResultsStream<TestProducer> {
    fn with<const N: usize>(
        send_results: [SendResult; N],
        close_result: Result<(), ProducerCloseError>,
        immediate_confirmation: Option<Confirmation>,
    ) -> ResultsStream<TestProducer> {
        let (messages_confirmed_tx, messages_confirmed_rx) = watch::channel(true);

        ResultsStream {
            producer: TestProducer {
                state: Arc::new(Mutex::new(TestProducerState::default())),
                send_results: Arc::new(Mutex::new(Vec::from(send_results))),
                close_result: Arc::new(Mutex::new(Some(close_result))),
                immediate_confirmation,
            },
            stats: Arc::new(Stats::new()),
            messages_confirmed_tx,
            messages_confirmed_rx,
        }
    }
}

// fn results_stream<'a, const N: usize>(
//     producer: &TestProducer<N>,
// ) -> (ResultsStream<TestProducer<N>>, Arc<Stats>) {
//     let stats = Arc::new(Stats::new());
//     let (messages_confirmed_tx, messages_confirmed_rx) = watch::channel(true);

//     (
//         ResultsStream {
//             producer: producer.clone(),
//             stats: stats.clone(),
//             messages_confirmed_tx,
//             messages_confirmed_rx,
//         },
//         stats,
//     )
// }

#[tokio::test]
async fn successful_send_records_message_and_statistics() {
    let mut stream = ResultsStream::with([], Ok(()), None);

    stream.send(b"message body").await;

    assert_eq!(
        stream.producer.sent_messages(),
        vec![b"message body".to_vec()]
    );
    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.bytes_sent, 12);
    assert_eq!(stream.stats.messages_failures, 0);
}

#[tokio::test]
async fn message_is_pending_before_producer_send_starts() {
    let stats = Arc::new(Stats::new());
    let (messages_confirmed_tx, messages_confirmed_rx) = watch::channel(true);
    let observation = Arc::new(Mutex::new(None));
    let producer = PendingStateObservingProducer {
        confirmation_state: messages_confirmed_rx.clone(),
        stats: stats.clone(),
        observation: observation.clone(),
    };
    let mut stream = ResultsStream {
        producer,
        stats,
        messages_confirmed_tx,
        messages_confirmed_rx,
    };

    stream.send(b"message").await;

    let observation = observation
        .lock()
        .unwrap()
        .expect("producer send was not called");
    assert_eq!(observation.messages_sent, 0);
    assert!(!observation.all_messages_confirmed);
}

#[tokio::test]
async fn record_messages_and_bytes_sent_for_successful_send() {
    let mut stream = ResultsStream::with([SendResult::Success], Ok(()), None);

    stream.send(b"message").await;

    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.bytes_sent, 7);
}

#[tokio::test]
async fn dont_record_messages_or_bytes_sent_for_failed_send() {
    let mut stream = ResultsStream::with([SendResult::Closed], Ok(()), None);

    stream.send(b"not sent").await;

    assert_eq!(stream.producer.send_calls(), 1);
    assert_eq!(stream.stats.messages_sent, 0);
    assert_eq!(stream.stats.bytes_sent, 0);
    assert_eq!(stream.stats.messages_failures, 1);
}

#[tokio::test(start_paused = true)]
async fn timeout_is_retried_and_success_is_counted_once() {
    let mut stream = ResultsStream::with(
        [
            SendResult::Timeout,
            SendResult::Timeout,
            SendResult::Success,
        ],
        Ok(()),
        None,
    );

    stream.send(b"eventually sent").await;

    assert_eq!(stream.producer.send_calls(), 3); // we tried to send 3 times
    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.bytes_sent, 15);
    assert_eq!(stream.stats.messages_failures, 0);
}

#[tokio::test(start_paused = true)]
async fn exhausted_timeout_retries_counts_one_failure() {
    let mut stream = ResultsStream::with([SendResult::Timeout; MAX_RETRIES + 1], Ok(()), None);

    stream.send(b"never sent").await;

    assert_eq!(stream.producer.send_calls(), MAX_RETRIES + 1);
    assert_eq!(stream.stats.messages_sent, 0);
    assert_eq!(stream.stats.bytes_sent, 0);
    assert_eq!(stream.stats.messages_failures, 1);
}

#[tokio::test]
async fn non_timeout_error_is_not_retried() {
    let mut stream = ResultsStream::with([SendResult::Closed, SendResult::Success], Ok(()), None);

    stream.send(b"message").await;

    assert_eq!(stream.producer.send_calls(), 1);
    assert_eq!(stream.stats.messages_failures, 1);
    assert_eq!(stream.stats.messages_sent, 0);
}

#[tokio::test]
async fn immediate_confirmation_does_not_leave_stale_state() {
    let mut stream = ResultsStream::with([], Ok(()), Some(Confirmation::Confirmed));

    stream.send(b"message").await;
    stream.wait_confirmation(Duration::from_millis(1)).await;

    assert_eq!(stream.stats.messages_sent, 1);
    assert_eq!(stream.stats.messages_confirmed, 1);
    assert_eq!(stream.stats.confirmation_wait_timeouts, 0);
}

// // TODO: how does this test work
// #[tokio::test]
// async fn wait_confirmation_blocks_until_confirmation_arrives() {
//     let mut stream = ResultsStream::with([SendResult::Success], Ok(()), None);

//     // let handle = TestProducer::builder().build();
//     // let (mut stream, stats) = results_stream(&handle);
//     stream.send(b"message").await;

//     let wait = stream.wait_confirmation(Duration::from_secs(1));
//     tokio::pin!(wait);
//     tokio::select! {
//         biased;
//         _ = &mut wait => panic!("confirmation wait completed while a message was outstanding"),
//         _ = tokio::task::yield_now() => {}
//     }

//     stream
//         .producer
//         .confirm(0, Ok(Confirmation::Confirmed))
//         .await;
//     wait.await;
//     assert_eq!(stream.stats.confirmation_wait_timeouts.get(), 0);
// }

// #[tokio::test]
// async fn wait_confirmation_waits_for_every_message() {
//     let handle = TestProducer::builder().build();
//     let (mut stream, stats) = results_stream(&handle);
//     stream.send(b"first").await;
//     stream.send(b"second").await;

//     handle.confirm(1, Ok(Confirmation::Confirmed)).await;
//     let wait = stream.wait_confirmation(Duration::from_secs(1));
//     tokio::pin!(wait);
//     tokio::select! {
//         biased;
//         _ = &mut wait => panic!("confirmation wait completed before every message resolved"),
//         _ = tokio::task::yield_now() => {}
//     }

//     handle.confirm(0, Ok(Confirmation::Confirmed)).await;
//     wait.await;
//     assert_eq!(stats.messages_confirmed.get(), 2);
// }

// #[tokio::test(start_paused = true)]
// async fn confirmation_wait_timeout_is_recorded() {
//     let handle = TestProducer::builder().build();
//     let (mut stream, stats) = results_stream(&handle);
//     stream.send(b"message").await;
//     // why does this test work? this shouldnt be timing out i would think
//     stream.wait_confirmation(Duration::from_secs(1)).await;

//     assert_eq!(stats.confirmation_wait_timeouts.get(), 1);
// }

// #[tokio::test(start_paused = true)]
// async fn unconfirmed_message_is_recorded_and_waits_until_timeout() {
//     let handle = TestProducer::builder().build();
//     let (mut stream, stats) = results_stream(&handle);
//     stream.send(b"message").await;
//     handle.confirm(0, Ok(Confirmation::Unconfirmed)).await;

//     stream.wait_confirmation(Duration::from_secs(1)).await;

//     assert_eq!(stats.messages_unconfirmed.get(), 1);
//     assert_eq!(stats.messages_confirmed.get(), 0);
//     assert_eq!(stats.confirmation_wait_timeouts.get(), 1);
// }

// #[tokio::test(start_paused = true)]
// async fn confirmation_callback_error_is_only_logged_and_waits_until_timeout() {
//     let handle = TestProducer::builder().build();
//     let (mut stream, stats) = results_stream(&handle);
//     stream.send(b"message").await;
//     handle.confirm(0, Err(ProducerPublishError::Timeout)).await;

//     stream.wait_confirmation(Duration::from_secs(1)).await;

//     assert_eq!(stats.messages_failures.get(), 0);
//     assert_eq!(stats.messages_confirmed.get(), 0);
//     assert_eq!(stats.confirmation_wait_timeouts.get(), 1);
// }

// #[tokio::test]
// async fn wait_confirmation_returns_immediately_when_nothing_was_sent() {
//     let handle = TestProducer::builder().build();
//     let (mut stream, stats) = results_stream(&handle);

//     stream.wait_confirmation(Duration::from_millis(1)).await;

//     assert_eq!(stats.confirmation_wait_timeouts.get(), 0);
// }

// #[tokio::test]
// async fn disconnect_waits_for_outstanding_confirmations_before_closing() {
//     let handle = TestProducer::builder().build();
//     let (mut stream, _stats) = results_stream(&handle);
//     stream.send(b"message").await;

//     let disconnect_task = tokio::spawn(stream.disconnect());
//     tokio::task::yield_now().await;
//     assert_eq!(handle.close_calls(), 0);

//     handle.confirm(0, Ok(Confirmation::Confirmed)).await;
//     disconnect_task.await.unwrap();
//     assert_eq!(handle.close_calls(), 1);
// }

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
