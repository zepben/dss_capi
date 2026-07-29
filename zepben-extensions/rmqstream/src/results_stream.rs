use rabbitmq_stream_client::{
    ConfirmationStatus, Environment, NoDedup, Producer,
    error::{ProducerCloseError, ProducerPublishError},
    types::{Message, ResponseCode},
};
use std::time::{Duration, Instant};
use tokio::sync::Notify;
use tracing::{debug, error, info, warn};
use tracing_log::log::trace;

use crate::{retry::with_retries, stats::Stats};

const BATCH_SIZE: usize = 100000;

pub struct ResultsStream {
    producer: Producer<NoDedup>,
    stats: Stats,

    /// A notification that can be awaited to check that all sent messages have
    /// been confirmed
    all_messages_confirmed: Notify,
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

                Ok(Self {
                    producer,
                    stats: Stats::new(),
                    all_messages_confirmed: Notify::new(),
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
                self.producer
                    .send(message.clone(), ResultsStream::on_confirm)
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

        self.stats.busy_time += start.elapsed();
        self.stats.total_messages += 1;
        self.stats.total_bytes += msg.len();
    }

    async fn on_confirm(result: Result<ConfirmationStatus, ProducerPublishError>) {
        todo!()
    }

    /// Wait for all messages to be confirmed.
    pub async fn wait_confirmation(&self, timeout: Duration) {
        match tokio::time::timeout(timeout, self.all_messages_confirmed.notified()).await {
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

        self.stats.log_summary();
    }
}
