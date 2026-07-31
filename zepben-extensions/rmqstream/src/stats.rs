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
use tracing::info;

/// Statistics for the performance and health of the results stream.
pub struct Stats {
    /// The metrics provider. This is retained so we can flush all metrics when exiting. This is
    /// `None` if metrics are not enabled.
    pub metrics_provider: Option<SdkMeterProvider>,

    busy_time: Mutex<Duration>,
    start_time: Instant,
    pub bytes_sent: ExposedCounter,
    pub messages_sent: ExposedCounter,
    /// The number of messages that fail to send after retries
    pub messages_failures: ExposedCounter,
    pub messages_confirmed: ExposedCounter,
    /// Explicitly unconfirmed messages
    pub messages_unconfirmed: ExposedCounter,

    /// The number of times that we have timed out waiting for all messages to be confirmed
    pub confirmation_wait_timeouts: ExposedCounter,
    /// The number of times that disconnecting from RabbitMQ has failed
    pub disconnects_failed: ExposedCounter,
}

impl Stats {
    pub fn new() -> Self {
        let metrics_provider = initialise_metrics();

        Self {
            metrics_provider,
            busy_time: Mutex::const_new(Duration::ZERO),
            start_time: Instant::now(),
            bytes_sent: ExposedCounter::new("bytes_sent"),
            messages_sent: ExposedCounter::new("messages_sent"),
            messages_failures: ExposedCounter::new("messages_failures"),
            messages_confirmed: ExposedCounter::new("messages_confirmed"),
            messages_unconfirmed: ExposedCounter::new("messages_unconfirmed"),
            confirmation_wait_timeouts: ExposedCounter::new("confirmation_wait_timeouts"),
            disconnects_failed: ExposedCounter::new("disconnects_failed"),
        }
    }

    pub async fn add_busy(&self, busy: Duration) {
        *self.busy_time.lock().await += busy
    }

    pub fn all_messages_confirmed(&self) -> bool {
        self.messages_confirmed.get() == self.messages_sent.get()
    }

    pub async fn log_summary(&self) {
        let total_messages = self.messages_sent.get();
        let seconds_elapsed = self.start_time.elapsed().as_secs_f64();
        let busy_percent = (self.busy_time.lock().await.as_secs_f64() / seconds_elapsed) * 100.0;
        let msg_per_sec = self.messages_sent.get() as f64 / seconds_elapsed;
        let bits_per_sec = 8.0 * self.bytes_sent.get() as f64 / seconds_elapsed;

        info!(
            "results stream statistics: {total_messages} total messages, {busy_percent}% busy, {msg_per_sec} msg/sec, {bits_per_sec} bits/sec",
        );
    }
}

/// An atomic counter that is also exposed as a metric.
pub struct ExposedCounter {
    count: AtomicU64,
    counter: Counter<u64>,
}

impl ExposedCounter {
    pub fn new(metric_name: &'static str) -> Self {
        const METER_NAME: &str = "meter_name";

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
