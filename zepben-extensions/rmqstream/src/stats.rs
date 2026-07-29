use std::time::{Duration, Instant};

use tracing::info;

pub struct Stats {
    pub busy_time: Duration,
    pub start_time: Instant,
    pub total_bytes: usize,
    pub total_messages: u64,
    pub confirmed_messages: u64,
}

impl Stats {
    pub fn new() -> Self {
        Self {
            busy_time: Duration::ZERO,
            start_time: Instant::now(),
            total_bytes: 0,
            total_messages: 0,
            confirmed_messages: 0,
        }
    }

    pub fn log_summary(&self) {
        let total_messages = self.total_messages;
        let seconds_elapsed = self.start_time.elapsed().as_secs_f64();
        let busy_percent = (self.busy_time.as_secs_f64() / seconds_elapsed) * 100.0;
        let msg_per_sec = self.total_messages as f64 / seconds_elapsed;
        let bits_per_sec = 8.0 * self.total_bytes as f64 / seconds_elapsed;

        info!(
            "results stream statistics: {total_messages} total messages, {busy_percent}% busy, {msg_per_sec} msg/sec, {bits_per_sec} bits/sec",
        );
    }
}
