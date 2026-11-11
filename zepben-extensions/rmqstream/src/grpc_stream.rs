use bytes::Bytes;
use std::error::Error;
use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio::sync::{mpsc, watch};
use tokio::task::JoinHandle;
use tokio::time::timeout;
use tokio_stream::wrappers::ReceiverStream;
use tonic::transport::{Channel, Endpoint};
use tonic_health::pb::{
    HealthCheckRequest, health_check_response::ServingStatus, health_client::HealthClient,
};
use tracing::{debug, error, info, warn};

use crate::{
    grpc_stream::{
        codec::EncodedBatch,
        proto::{StreamSummary, open_dss_results_stream_client::OpenDssResultsStreamClient},
    },
    retry::{BACKOFF_DELAY, with_retries_from},
    stats::Stats,
};

pub mod codec;

#[allow(dead_code, clippy::all)]
pub mod proto {
    tonic::include_proto!("zepben.protobuf.hc.opendss");
}

/// The number of batches that can be queued for the stream before [GrpcResultsStream::send] blocks.
/// gRPC flow control provides backpressure beyond this, so this is kept small.
const CHANNEL_CAPACITY: usize = 16;

/// The timeout for each attempt to connect to, and health check, the result processor.
const CONNECT_TIMEOUT: Duration = Duration::from_secs(10);

/// The timeout when waiting for an unfinished stream to be cancelled.
const CANCEL_TIMEOUT: Duration = Duration::from_secs(5);

/// The outcome of the `StreamResults` call.
type CallResult = Result<tonic::Response<StreamSummary>, tonic::Status>;

/// A client stream of results to the result processor's `OpenDssResultsStream` gRPC service.
pub struct GrpcResultsStream {
    /// The address of the result processor, used for logging.
    address: String,
    /// The sending side of the request stream. Dropping this half-closes the stream.
    sender: Option<mpsc::Sender<EncodedBatch>>,
    /// The task running the `StreamResults` call. `None` once the call's result has been taken.
    call: Option<JoinHandle<CallResult>>,
    stats: Arc<Stats>,
}

impl GrpcResultsStream {
    /// Connect to the result processor at `address` (`host:port`), and open the results stream.
    ///
    /// # Panics
    ///
    /// This function panics if the result processor is not healthy after retries.
    pub async fn new(address: &str) -> GrpcResultsStream {
        Self::connect(address, BACKOFF_DELAY).await
    }

    /// As [GrpcResultsStream::new], with the given initial delay between connection attempts.
    pub(crate) async fn connect(address: &str, initial_retry_delay: Duration) -> GrpcResultsStream {
        debug!("connecting to gRPC results stream at {address}");

        let endpoint = match Endpoint::from_shared(format!("http://{address}")) {
            Ok(endpoint) => endpoint.connect_timeout(CONNECT_TIMEOUT),
            Err(e) => panic!("invalid result processor address `{address}`: {e}"),
        };

        let channel = match with_retries_from(
            initial_retry_delay,
            "connecting to the result processor",
            |_: &String| true,
            || {},
            || connect_healthy(&endpoint),
        )
        .await
        {
            Ok(channel) => channel,
            Err(e) => panic!("failed to connect to the result processor at {address}: {e}"),
        };

        let (sender, receiver) = mpsc::channel(CHANNEL_CAPACITY);
        let mut client = OpenDssResultsStreamClient::new(channel);
        let call =
            tokio::spawn(async move { client.stream_results(ReceiverStream::new(receiver)).await });

        info!("connected to gRPC results stream at {address}");

        // inflight messages are not tracked for gRPC, as there are no per-message confirmations.
        let (inflight_tx, _) = watch::channel(0);

        GrpcResultsStream {
            address: address.to_string(),
            sender: Some(sender),
            call: Some(call),
            stats: Arc::new(Stats::new(inflight_tx)),
        }
    }

    /// Send a message to the results stream. This blocks while the result processor is behind,
    /// under gRPC flow control.
    ///
    /// # Panics
    ///
    /// This function panics if the stream has failed, or has already been finished.
    pub async fn send(&mut self, msg: &[u8]) {
        let Some(sender) = self.sender.as_ref() else {
            panic!(
                "failed to stream message to {}: the stream has already been finished",
                self.address
            )
        };

        let start = Instant::now();
        let result = sender.send(EncodedBatch(Bytes::copy_from_slice(msg))).await;
        self.stats.add_busy(start.elapsed()).await;

        match result {
            Ok(()) => {
                self.stats.messages_sent.add(1);
                self.stats.bytes_sent.add(msg.len() as u64);
            }
            Err(_) => {
                // the request stream has been dropped, so the call has ended. find out why.
                let outcome = describe(&self.call_result().await);

                self.stats.messages_failed.add(1);
                self.stats.flush();
                error!(
                    "failed to stream message to {} after sending {} messages ({} bytes): {outcome}",
                    self.address,
                    self.stats.messages_sent.get(),
                    self.stats.bytes_sent.get(),
                );
                panic!("failed to stream message to {}: {outcome}", self.address)
            }
        }
    }

    /// Half-close the results stream, and wait for the result processor's [StreamSummary].
    ///
    /// Returns the `StreamStatus` value of the summary, or -1 if the stream failed.
    pub async fn finish(&mut self) -> i32 {
        if self.sender.take().is_none() {
            error!("gRPC results stream to {} already finished", self.address);
            return -1;
        }

        match self.call_result().await {
            Ok(response) => {
                let summary = response.into_inner();
                info!(
                    "finished gRPC results stream to {} with status {} after sending {} messages ({} bytes)",
                    self.address,
                    status_name(summary.status),
                    self.stats.messages_sent.get(),
                    self.stats.bytes_sent.get(),
                );
                summary.status
            }
            Err(status) => {
                error!(
                    "gRPC results stream to {} failed after sending {} messages ({} bytes): {status}",
                    self.address,
                    self.stats.messages_sent.get(),
                    self.stats.bytes_sent.get(),
                );
                -1
            }
        }
    }

    /// Release the results stream. If the stream was not finished, it is cancelled rather than
    /// half-closed, so the result processor does not treat the results as complete.
    pub async fn disconnect(mut self) {
        if let Some(call) = self.call.take() {
            if self.sender.is_some() {
                warn!(
                    "gRPC results stream to {} was not finished, cancelling it",
                    self.address
                );
            }

            // cancel the call, which resets the stream. the sender must not be dropped until the
            // request stream has been dropped by the reset, as dropping it first could half-close
            // the stream, which looks like a successful end of results to the result processor.
            call.abort();
            let _ = call.await;
            if let Some(sender) = self.sender.as_ref()
                && timeout(CANCEL_TIMEOUT, sender.closed()).await.is_err()
            {
                error!(
                    "timed out waiting for gRPC results stream to {} to be cancelled",
                    self.address
                );
            }
        }
        self.sender = None;

        info!("disconnected from gRPC results stream to {}", self.address);
        self.stats.flush();
        self.stats.log_summary().await;
    }

    /// Wait for, and take, the result of the `StreamResults` call.
    async fn call_result(&mut self) -> CallResult {
        match self.call.take() {
            Some(call) => call.await.unwrap_or_else(|e| {
                Err(tonic::Status::unknown(format!(
                    "results stream task failed: {e}"
                )))
            }),
            None => Err(tonic::Status::failed_precondition(
                "results stream has already ended",
            )),
        }
    }
}

/// Connect to the result processor, and check that it is serving.
async fn connect_healthy(endpoint: &Endpoint) -> Result<Channel, String> {
    let channel = endpoint.connect().await.map_err(|e| error_chain(&e))?;

    let request = HealthCheckRequest {
        service: String::new(),
    };
    let response = timeout(
        CONNECT_TIMEOUT,
        HealthClient::new(channel.clone()).check(request),
    )
    .await
    .map_err(|_| String::from("timed out waiting for health check"))?
    .map_err(|status| format!("health check failed: {status}"))?;

    match response.into_inner().status() {
        ServingStatus::Serving => Ok(channel),
        status => Err(format!("health status is {}", status.as_str_name())),
    }
}

/// Describe how a stream ended, when it was not expected to have ended.
fn describe(result: &CallResult) -> String {
    match result {
        Ok(response) => format!(
            "the result processor ended the stream early with status {}",
            status_name(response.get_ref().status)
        ),
        Err(status) => status.to_string(),
    }
}

fn status_name(status: i32) -> String {
    match proto::StreamStatus::try_from(status) {
        Ok(status) => status.as_str_name().to_string(),
        Err(_) => status.to_string(),
    }
}

/// Format an error with all of its sources, as transport errors are not descriptive by themselves.
fn error_chain(e: &dyn Error) -> String {
    let mut message = e.to_string();
    let mut source = e.source();
    while let Some(e) = source {
        message.push_str(&format!(": {e}"));
        source = e.source();
    }
    message
}

#[cfg(test)]
mod tests;
