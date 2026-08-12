//! Statistics for the performance of the results stream.
//!
//! Uses of atomics in this module are very conservative. [Ordering::SeqCst] is used here as a safe
//! option, to avoid having to check all atomic uses for ordering constraints. It would be possible
//! to use a more relaxed memory ordering.

use crate::monitoring::initialise_metrics;

use opentelemetry::{global, metrics::Counter};
use opentelemetry_sdk::metrics::SdkMeterProvider;
use std::sync::atomic::{AtomicU64, Ordering::SeqCst};
use std::time::{Duration, Instant};
use tokio::sync::Mutex;
use tokio::sync::watch::Sender;
use tracing::info;

/// Statistics for the performance and health of the results stream.
pub struct Stats {
    /// The metrics provider. This is retained so we can flush all metrics when exiting. This is
    /// `None` if metrics are not enabled.
    pub metrics_provider: Option<SdkMeterProvider>,

    busy_time: Mutex<Duration>,
    start_time: Instant,

    /// The number of bytes that have been asked to be sent by unique messages.
    pub bytes_sent: ExposedCounter,
    /// The number of messages unique messages that we have asked to be sent. These may not have all
    /// made it to RabbitMQ, or been confirmed.
    pub messages_sent: ExposedCounter,
    /// The number of messages that fail to send after retries
    pub messages_failed: ExposedCounter,
    pub messages_confirmed: ExposedCounter,
    /// Explicitly unconfirmed messages
    pub messages_unconfirmed: ExposedCounter,

    /// The number of times that we have timed out waiting for no inflight messages
    pub no_inflight_timeouts: ExposedCounter,
    /// The number of times that disconnecting from RabbitMQ has failed
    pub disconnects_failed: ExposedCounter,

    /// The number of inflight messages
    inflight_messages: Sender<u64>,
}

impl Stats {
    pub fn new(sender: Sender<u64>) -> Self {
        let metrics_provider = initialise_metrics();

        Self {
            metrics_provider,
            busy_time: Mutex::const_new(Duration::ZERO),
            start_time: Instant::now(),
            bytes_sent: ExposedCounter::new("bytes_sent"),
            messages_sent: ExposedCounter::new("messages_sent"),
            messages_failed: ExposedCounter::new("messages_failed"),
            messages_confirmed: ExposedCounter::new("messages_confirmed"),
            messages_unconfirmed: ExposedCounter::new("messages_unconfirmed"),
            no_inflight_timeouts: ExposedCounter::new("no_inflight_timeouts"),
            disconnects_failed: ExposedCounter::new("disconnects_failed"),
            inflight_messages: sender,
        }
    }

    pub fn increment_messages_sent(&self) {
        self.messages_sent.add(1);
        self.inflight_messages.send_modify(|x| *x += 1);
    }

    pub fn increment_messages_confirmed(&self) {
        self.messages_confirmed.add(1);
        self.inflight_messages.send_modify(|x| *x -= 1);
    }

    pub fn increment_messages_unconfirmed(&self) {
        self.messages_unconfirmed.add(1);
        self.inflight_messages.send_modify(|x| *x -= 1);
    }

    pub fn increment_messages_failed(&self) {
        self.messages_failed.add(1);
        self.inflight_messages.send_modify(|x| *x -= 1);
    }

    pub async fn add_busy(&self, busy: Duration) {
        *self.busy_time.lock().await += busy
    }

    pub async fn log_summary(&self) {
        let total_messages = self.messages_sent.get();
        let seconds_elapsed = self.start_time.elapsed().as_secs_f64();
        let busy_percent = (self.busy_time.lock().await.as_secs_f64() / seconds_elapsed) * 100.0;
        let msg_per_sec = self.messages_sent.get() as f64 / seconds_elapsed;
        let bits_per_sec = 8.0 * self.bytes_sent.get() as f64 / seconds_elapsed;

        info!(
            "results stream statistics: {total_messages} total messages, {busy_percent:.2}% busy, {msg_per_sec:.2} msg/sec, {bits_per_sec:.2} bits/sec",
        );
    }

    /// Flush the metrics tracking these statistics.
    pub fn flush(&self) {
        if let Some(ref metrics_provider) = self.metrics_provider {
            let _ = metrics_provider.force_flush();
        }
    }
}

/// An atomic counter that is also exposed as a metric.
#[derive(Debug)]
pub struct ExposedCounter {
    count: AtomicU64,
    counter: Counter<u64>,
}

impl ExposedCounter {
    pub fn new(metric_name: &'static str) -> Self {
        const METER_NAME: &str = "results_stream";

        let meter = global::meter(METER_NAME);
        let counter = meter.u64_counter(metric_name).build();

        Self {
            count: AtomicU64::new(0),
            counter,
        }
    }

    pub fn add(&self, x: u64) {
        self.count.fetch_add(x, SeqCst);
        self.counter.add(x, &[]);
    }

    pub fn get(&self) -> u64 {
        self.count.load(SeqCst)
    }
}

impl PartialEq<u64> for ExposedCounter {
    fn eq(&self, other: &u64) -> bool {
        self.count.load(SeqCst) == *other
    }
}

impl PartialEq for ExposedCounter {
    fn eq(&self, other: &Self) -> bool {
        self.count.load(SeqCst) == other.count.load(SeqCst)
    }
}
