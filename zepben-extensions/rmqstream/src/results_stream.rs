use async_trait::async_trait;
use rabbitmq_stream_client::{
    Environment, NoDedup, OnClosed, Producer,
    error::{ProducerCloseError, ProducerPublishError},
    types::{Message, ResponseCode},
};
use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio::sync::watch::{self, Receiver};
use tracing::{debug, error, info, warn};
use tracing_log::log::trace;

use crate::{
    producer::{Confirmation, ConfirmationResult, StreamProducer},
    retry::with_retries,
    stats::Stats,
};

const BATCH_SIZE: usize = 100000;

pub struct ResultsStream<T: StreamProducer> {
    producer: T,
    stats: Arc<Stats>,

    /// The number of inflight messages.
    inflight_messages: Receiver<u64>,
}

impl ResultsStream<Producer<NoDedup>> {
    /// Create the [ResultsStream]. This will open an internal connection to RabbitMQ, and then
    /// create the stream with the given connection options.
    ///
    /// # Panics
    ///
    /// This function panics if a connection to RabbitMQ cannot be established.
    pub async fn new(
        host: &str,
        port: u16,
        username: &str,
        password: &str,
        stream_name: &str,
        heartbeat: u32,
    ) -> ResultsStream<Producer<NoDedup>> {
        debug!("connecting to RabbitMQ stream `{username}@{host}:{port}` on stream {stream_name}");

        match with_retries(
            "connecting to RabbitMQ",
            |_: &String| true,
            || async {
                let (tx, rx) = watch::channel(0);
                let stats = Arc::new(Stats::new(tx));

                let environment = Environment::builder()
                    .host(host)
                    .port(port)
                    .username(username)
                    .password(password)
                    .heartbeat(heartbeat)
                    .load_balancer_mode(true)
                    .build()
                    .await
                    .map_err(|e| e.to_string())?;

                let producer = environment
                    .producer()
                    .batch_size(BATCH_SIZE)
                    .on_closed(Box::new(OnClosedHandler {
                        stats: stats.clone(),
                    }))
                    .build(stream_name)
                    .await
                    .map_err(|e| e.to_string())?;

                info!(
                    "connected to RabbitMQ {username}@{host}:{port}, for stream '{stream_name}'."
                );

                Ok(Self {
                    producer,
                    stats,
                    inflight_messages: rx,
                })
            },
        )
        .await
        {
            Ok(x) => x,
            Err(e) => panic!("failed to connect to RabbitMQ: {e:?}"),
        }
    }
}

impl<T: StreamProducer + 'static> ResultsStream<T> {
    /// Send a message to the results stream.
    pub async fn send(&mut self, msg: &[u8]) {
        let result = self
            .send_internal(msg, async |message| {
                let stats = self.stats.clone();

                self.producer
                    .send(
                        message,
                        Box::new(|confirmation| {
                            Box::pin(async { record_confirmation(stats, confirmation) })
                        }),
                    )
                    .await?;
                Ok(())
            })
            .await;

        if let Err(e) = result {
            warn!("failed to stream message: {e}");
            self.stats.increment_messages_failed();
        }
    }

    /// Send a message to the results stream and block publishing until the message is confirmed, or
    /// unconfirmed.
    pub async fn send_and_wait_confirmation(&self, msg: &[u8]) {
        let confirmation = self
            .send_internal(msg, async |message| {
                self.producer.send_and_wait_confirmation(message).await
            })
            .await;

        record_confirmation(self.stats.clone(), confirmation);
    }

    async fn send_internal<U>(
        &self,
        msg: &[u8],
        f: impl AsyncFn(Message) -> Result<U, ProducerPublishError>,
    ) -> Result<U, ProducerPublishError> {
        let start = Instant::now();
        let message = Message::builder().body(msg).build();

        self.stats.increment_messages_sent();
        self.stats.bytes_sent.add(msg.len() as u64);

        let result = with_retries(
            "publishing message",
            |e| matches!(e, ProducerPublishError::Timeout),
            async || f(message.clone()).await,
        )
        .await;

        self.stats.add_busy(start.elapsed()).await;
        result
    }

    /// Wait for all messages to be confirmed, unconfirmed, or failed.
    pub async fn wait_no_inflight(&mut self, timeout: Duration) -> Result<(), ()> {
        match tokio::time::timeout(timeout, self.inflight_messages.wait_for(|&x| x == 0)).await {
            Ok(_) => Ok(()),
            Err(_) => {
                self.stats.confirmation_wait_timeouts.add(1);
                error!(
                    "some messages still inflight after waiting {}ms",
                    timeout.as_millis()
                );
                Err(())
            }
        }
    }

    /// Disconnect from the results stream, and log a summary of stream statistics
    pub async fn disconnect(self) {
        match self.producer.close().await {
            Ok(_) => info!("disconnected from RabbitMQ"),
            Err(ProducerCloseError::Close {
                status: ResponseCode::PublisherDoesNotExist,
                ..
            }) => warn!("stream already closed (publisher)"),
            Err(_) => {
                self.stats.disconnects_failed.add(1);
                error!("failed to disconnect from RabbitMQ stream");
            }
        }

        if let Some(ref metrics_provider) = self.stats.metrics_provider {
            let _ = metrics_provider.force_flush(); // ignore errors when flushing metrics
        }
        self.stats.log_summary().await;
    }
}

/// Record the given confirmation. This updates statistics, and by extension updates result stream
/// inflight status.
fn record_confirmation(stats: Arc<Stats>, confirmation: ConfirmationResult) {
    match confirmation {
        Ok(Confirmation::Confirmed) => {
            stats.increment_messages_confirmed();
            trace!("streamed a message")
        }
        Ok(Confirmation::Unconfirmed) => {
            stats.increment_messages_unconfirmed();
            warn!("failed to stream message: unconfirmed")
        }
        Err(e) => {
            stats.increment_messages_failed();
            warn!("failed to stream message: {e}")
        }
    }
}

struct OnClosedHandler {
    stats: Arc<Stats>,
}

#[async_trait]
impl OnClosed for OnClosedHandler {
    async fn on_closed(&self, unconfirmed: Vec<Message>) {
        if !unconfirmed.is_empty() {
            warn!(
                "{} unconfirmed messages may be discarded",
                unconfirmed.len()
            );
        }

        for _ in unconfirmed {
            self.stats.increment_messages_unconfirmed();
        }
    }
}

#[cfg(test)]
mod tests;
