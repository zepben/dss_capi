//! Statistics for the performance of the results stream.
//!
//! Uses of atomics in this module are very conservative. [Ordering::SeqCst] is used here as a safe
//! option, to avoid having to check all atomic uses for ordering constraints. It would be possible
//! to use a more relaxed memory ordering.

use std::{
    sync::atomic::{
        AtomicU64, AtomicUsize,
        Ordering::{self, SeqCst},
    },
    time::{Duration, Instant},
};

use tokio::sync::Mutex;
use tracing::info;

/// Global statistics for the performance and health of the results stream.
pub struct Stats {
    busy_time: Mutex<Duration>,
    start_time: Instant,
    send_bytes: AtomicUsize,
    sent_messages: Messages,
    confirmed_messages: Messages,
    /// Explicitly unconfirmed messages
    unconfirmed_messages: Messages,
}

impl Stats {
    pub fn new() -> Self {
        Self {
            busy_time: Mutex::const_new(Duration::ZERO),
            start_time: Instant::now(),
            send_bytes: AtomicUsize::new(0),
            sent_messages: Messages::new(),
            confirmed_messages: Messages::new(),
            unconfirmed_messages: Messages::new(),
        }
    }

    pub async fn add_busy(&self, busy: Duration) {
        *self.busy_time.lock().await += busy
    }

    pub fn add_sent_bytes(&self, bytes: usize) {
        self.send_bytes.fetch_add(bytes, Ordering::SeqCst);
    }

    pub fn increment_sent(&self) {
        self.sent_messages.increment();
    }

    pub fn increment_confirmed(&self) {
        self.confirmed_messages.increment();
    }

    pub fn increment_unconfirmed(&self) {
        self.unconfirmed_messages.increment();
    }

    pub fn all_messages_confirmed(&self) -> bool {
        self.confirmed_messages.get() == self.sent_messages.get()
    }

    pub async fn log_summary(&self) {
        let total_messages = self.sent_messages.get();
        let seconds_elapsed = self.start_time.elapsed().as_secs_f64();
        let busy_percent = (self.busy_time.lock().await.as_secs_f64() / seconds_elapsed) * 100.0;
        let msg_per_sec = self.sent_messages.get() as f64 / seconds_elapsed;
        let bits_per_sec = 8.0 * self.send_bytes.load(Ordering::SeqCst) as f64 / seconds_elapsed;

        info!(
            "results stream statistics: {total_messages} total messages, {busy_percent}% busy, {msg_per_sec} msg/sec, {bits_per_sec} bits/sec",
        );
    }
}

pub struct Messages {
    count: AtomicU64,
}

impl Messages {
    pub const fn new() -> Self {
        Self {
            count: AtomicU64::new(0),
        }
    }

    pub fn increment(&self) {
        self.count.fetch_add(1, SeqCst);
    }

    pub fn get(&self) -> u64 {
        self.count.load(SeqCst)
    }
}
