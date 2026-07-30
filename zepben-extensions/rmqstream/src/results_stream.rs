use rabbitmq_stream_client::{
    ConfirmationStatus, Environment, NoDedup, Producer,
    error::{ProducerCloseError, ProducerPublishError},
    types::{Message, ResponseCode},
};
use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio::sync::watch::{self, Receiver, Sender};
use tracing::{debug, error, info, warn};
use tracing_log::log::trace;

use crate::{retry::with_retries, stats::Stats};

const BATCH_SIZE: usize = 100000;

pub struct ResultsStream {
    producer: Producer<NoDedup>,
    stats: Arc<Stats>,

    messages_confirmed_tx: Sender<bool>,
    messages_confirmed_rx: Receiver<bool>,
}

impl ResultsStream {
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
    ) -> Self {
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

    /// Send a message to the results stream.
    pub async fn send(&mut self, msg: &[u8]) {
        let start = Instant::now();
        let message = Message::builder().body(msg).build();

        match with_retries(
            "publishing message",
            |e| matches!(e, ProducerPublishError::Timeout),
            || {
                let confirmation_tx = self.messages_confirmed_tx.clone();
                let stats = self.stats.clone();

                self.producer.send(message.clone(), |confirmation| {
                    ResultsStream::on_confirm(confirmation_tx, stats, confirmation)
                })
            },
        )
        .await
        {
            Ok(_) => trace!(
                "streamed a message containing {} bytes",
                message.data().map_or(0, |data| data.len())
            ),
            Err(_) => todo!("log an error here, and expose the failure in a metric"),
        };

        self.stats.add_busy(start.elapsed()).await;
        self.stats.increment_sent();
        self.stats.add_sent_bytes(msg.len());
    }

    async fn on_confirm(
        tx: Sender<bool>,
        stats: Arc<Stats>,
        result: Result<ConfirmationStatus, ProducerPublishError>,
    ) {
        match result {
            Ok(status) if status.confirmed() => stats.increment_confirmed(),
            Ok(_) => stats.increment_unconfirmed(),
            Err(error) => todo!("log error and potentially expose failure"),
        }

        // ignore the possibility that the channel is closed
        let _ = tx.send(stats.all_messages_confirmed());
    }

    /// Wait for all messages to be confirmed.
    pub async fn wait_confirmation(&mut self, timeout: Duration) {
        match tokio::time::timeout(timeout, self.messages_confirmed_rx.wait_for(|&x| x)).await {
            Ok(_) => (),
            Err(_) => {
                // TODO: add a metric to track how often this happens
                error!(
                    "failed to confirm all RabbitMQ stream messages within {}ms of finishing",
                    timeout.as_millis()
                )
            }
        }
    }

    /// Disconnect from the results stream, and log a summary of stream statistics
    pub async fn disconnect(self) {
        match self.producer.close().await {
            Ok(_) => (),
            Err(ProducerCloseError::Close {
                status: ResponseCode::PublisherDoesNotExist,
                ..
            }) => {
                warn!("stream already closed (publisher)")
            }
            Err(_) => {
                todo!("log a warning and add to a metric here so we can see how often this happens")
            }
        }
        info!("disconnected from RabbitMQ");

        self.stats.log_summary().await;
    }
}
