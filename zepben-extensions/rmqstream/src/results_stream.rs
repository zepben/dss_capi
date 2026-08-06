use async_trait::async_trait;
use rabbitmq_stream_client::{
    Environment, NoDedup, OnClosed, Producer,
    error::{ProducerCloseError, ProducerPublishError},
    types::{Message, ResponseCode},
};
use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio::sync::watch::{self, Receiver, Sender};
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

    messages_confirmed_tx: Sender<bool>,
    messages_confirmed_rx: Receiver<bool>,
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
                    .on_closed(Box::new(OnClosedHandler))
                    .build(stream_name)
                    .await
                    .map_err(|e| e.to_string())?;

                info!(
                    "connected to RabbitMQ {username}@{host}:{port}, for stream '{stream_name}'."
                );

                let (tx, rx) = watch::channel(true);

                Ok(Self {
                    producer,
                    stats: Arc::new(Stats::new()),
                    messages_confirmed_tx: tx,
                    messages_confirmed_rx: rx,
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
        let start = Instant::now();
        let message = Message::builder().body(msg).build();

        // mark that not all messages are confirmed, while we are in progress sending this message
        let _ = self.messages_confirmed_tx.send(false);

        let result = with_retries(
            "publishing message",
            |e| matches!(e, ProducerPublishError::Timeout),
            async || {
                let confirmation_tx = self.messages_confirmed_tx.clone();
                let stats = self.stats.clone();

                self.producer
                    .send(
                        &message,
                        Box::new(|confirmation| {
                            Box::pin(ResultsStream::<T>::on_confirm(
                                confirmation_tx,
                                stats,
                                confirmation,
                            ))
                        }),
                    )
                    .await?;
                self.stats.messages_sent.add(1);
                self.stats.bytes_sent.add(msg.len() as u64);
                Ok(())
            },
        )
        .await;

        self.update_all_messages_confirmed();

        match result {
            Ok(_) => trace!("streamed a message containing {} bytes", msg.len()),
            Err(e) => {
                warn!("failed to stream message: {e}");
                self.stats.messages_failures.add(1);
            }
        };

        self.stats.add_busy(start.elapsed()).await;
    }

    /// Update the state of the [ResultsStream] to note that a new message has been sent.
    ///
    /// This refreshes the status of if all messages have been confirmed.
    fn update_all_messages_confirmed(&self) {
        let _ = self
            .messages_confirmed_tx
            .send(self.stats.all_messages_confirmed()); // ignore the possibility that the channel is closed
    }

    async fn on_confirm(tx: Sender<bool>, stats: Arc<Stats>, result: ConfirmationResult) {
        match result {
            Ok(Confirmation::Confirmed) => stats.messages_confirmed.add(1),
            Ok(Confirmation::Unconfirmed) => stats.messages_unconfirmed.add(1),
            Err(error) => {
                debug!("failure during message confirmation: {error}");
                stats.messages_failures.add(1);
            }
        }

        // ignore the possibility that the channel is closed
        let _ = tx.send(stats.all_messages_confirmed());
    }

    /// Wait for all messages to be confirmed.
    pub async fn wait_confirmation(&mut self, timeout: Duration) -> Result<(), ()> {
        match tokio::time::timeout(timeout, self.messages_confirmed_rx.wait_for(|&x| x)).await {
            Ok(_) => Ok(()),
            Err(_) => {
                self.stats.confirmation_wait_timeouts.add(1);
                error!(
                    "failed to confirm all RabbitMQ stream messages within {}ms",
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

struct OnClosedHandler;

#[async_trait]
impl OnClosed for OnClosedHandler {
    async fn on_closed(&self, unconfirmed: Vec<Message>) {
        if !unconfirmed.is_empty() {
            warn!("discarding {} unconfirmed messages", unconfirmed.len())
        }
    }
}

#[cfg(test)]
mod tests;
