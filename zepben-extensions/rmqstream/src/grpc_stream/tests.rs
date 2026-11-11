use super::*;
use crate::grpc_stream::proto::{
    StreamStatus,
    open_dss_results_stream_server::{OpenDssResultsStream, OpenDssResultsStreamServer},
};
use crate::retry::MAX_RETRIES;

use http_body::Frame;
use ntest::timeout;
use std::any::Any;
use std::pin::Pin;
use std::sync::Mutex;
use std::sync::atomic::{AtomicUsize, Ordering::SeqCst};
use std::task::{Context, Poll, ready};
use tokio::runtime::Runtime;
use tokio::sync::Notify;
use tonic::body::Body;
use tonic::transport::{Server, server::TcpIncoming};
use tonic::{Code, Request, Response, Status, Streaming};
use tonic_health::pb::{
    HealthCheckResponse,
    health_server::{Health, HealthServer},
};
use tower::ServiceBuilder;

const RETRY_DELAY: Duration = Duration::from_millis(10);

/// A result processor's `OpenDssResultsStream` service.
#[derive(Clone)]
struct TestResults {
    received: Arc<Mutex<Vec<Bytes>>>,
    /// How the request body ended: `Ok` on a half-close. This is recorded from the HTTP body, as
    /// tonic's `Streaming` reports a cancelled request as a clean end of stream.
    ended: Arc<Mutex<Option<Result<(), Code>>>>,
    /// The status to respond with, once the stream is half-closed.
    status: StreamStatus,
    /// Fail the call with `UNAVAILABLE` after this many messages are received.
    fail_after: Option<usize>,
    /// Don't read any messages until this is notified.
    gate: Option<Arc<Notify>>,
}

impl TestResults {
    fn new(status: StreamStatus) -> Self {
        TestResults {
            received: Arc::new(Mutex::new(Vec::new())),
            ended: Arc::new(Mutex::new(None)),
            status,
            fail_after: None,
            gate: None,
        }
    }

    fn received(&self) -> Vec<Bytes> {
        self.received.lock().unwrap().clone()
    }

    fn ended(&self) -> Option<Result<(), Code>> {
        *self.ended.lock().unwrap()
    }
}

#[tonic::async_trait]
impl OpenDssResultsStream for TestResults {
    async fn stream_results(
        &self,
        request: Request<Streaming<EncodedBatch>>,
    ) -> Result<Response<StreamSummary>, Status> {
        if let Some(gate) = &self.gate {
            gate.notified().await;
        }

        let mut stream = request.into_inner();
        loop {
            match stream.message().await {
                Ok(Some(EncodedBatch(batch))) => {
                    let received = {
                        let mut received = self.received.lock().unwrap();
                        received.push(batch);
                        received.len()
                    };
                    if self.fail_after == Some(received) {
                        return Err(Status::unavailable("going away"));
                    }
                }
                Ok(None) => break,
                Err(status) => return Err(status),
            }
        }

        Ok(Response::new(StreamSummary {
            status: self.status as i32,
        }))
    }
}

/// A request body that records how it ended.
struct RecordingBody {
    inner: Body,
    ended: Arc<Mutex<Option<Result<(), Code>>>>,
}

impl http_body::Body for RecordingBody {
    type Data = Bytes;
    type Error = Status;

    fn poll_frame(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Result<Frame<Bytes>, Status>>> {
        let frame = ready!(Pin::new(&mut self.inner).poll_frame(cx));
        match &frame {
            None => *self.ended.lock().unwrap() = Some(Ok(())),
            Some(Err(status)) => *self.ended.lock().unwrap() = Some(Err(status.code())),
            Some(Ok(_)) => {}
        }
        Poll::Ready(frame)
    }
}

/// A health service which responds with the given statuses in order, repeating the last one.
#[derive(Clone)]
struct TestHealth {
    statuses: Vec<ServingStatus>,
    checks: Arc<AtomicUsize>,
}

impl TestHealth {
    fn new<const N: usize>(statuses: [ServingStatus; N]) -> Self {
        TestHealth {
            statuses: Vec::from(statuses),
            checks: Arc::new(AtomicUsize::new(0)),
        }
    }

    fn serving() -> Self {
        Self::new([ServingStatus::Serving])
    }

    fn checks(&self) -> usize {
        self.checks.load(SeqCst)
    }
}

#[tonic::async_trait]
impl Health for TestHealth {
    async fn check(
        &self,
        request: Request<HealthCheckRequest>,
    ) -> Result<Response<HealthCheckResponse>, Status> {
        assert_eq!("", request.get_ref().service);

        let check = self.checks.fetch_add(1, SeqCst);
        let status = self.statuses[check.min(self.statuses.len() - 1)];
        Ok(Response::new(HealthCheckResponse {
            status: status as i32,
        }))
    }

    type WatchStream = tokio_stream::Empty<Result<HealthCheckResponse, Status>>;

    async fn watch(
        &self,
        _: Request<HealthCheckRequest>,
    ) -> Result<Response<Self::WatchStream>, Status> {
        Err(Status::unimplemented("watch is not used"))
    }
}

/// A gRPC server on 127.0.0.1, running on its own runtime so it can be killed, dropping all of its
/// connections.
struct TestServer {
    address: String,
    runtime: Option<Runtime>,
}

impl TestServer {
    fn start(results: TestResults, health: TestHealth) -> TestServer {
        let runtime = tokio::runtime::Builder::new_multi_thread()
            .worker_threads(1)
            .enable_all()
            .build()
            .unwrap();

        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        listener.set_nonblocking(true).unwrap();
        let address = listener.local_addr().unwrap().to_string();

        let ended = results.ended.clone();
        let record_ended =
            ServiceBuilder::new().map_request(move |request: http::Request<Body>| {
                if request.uri().path().ends_with("/StreamResults") {
                    let ended = ended.clone();
                    request.map(|inner| Body::new(RecordingBody { inner, ended }))
                } else {
                    request
                }
            });

        runtime.spawn(async move {
            let listener = tokio::net::TcpListener::from_std(listener).unwrap();
            Server::builder()
                .layer(record_ended)
                .add_service(HealthServer::new(health))
                .add_service(OpenDssResultsStreamServer::new(results))
                .serve_with_incoming(TcpIncoming::from(listener))
                .await
                .unwrap();
        });

        TestServer {
            address,
            runtime: Some(runtime),
        }
    }

    fn kill(&mut self) {
        if let Some(runtime) = self.runtime.take() {
            runtime.shutdown_background();
        }
    }
}

impl Drop for TestServer {
    fn drop(&mut self) {
        self.kill();
    }
}

async fn connect(server: &TestServer) -> GrpcResultsStream {
    GrpcResultsStream::connect(&server.address, RETRY_DELAY).await
}

/// Run `f` as a task, and return its panic message.
async fn expect_panic<F: Future<Output = ()> + Send + 'static>(f: F) -> String {
    match tokio::spawn(f).await {
        Ok(()) => panic!("expected a panic"),
        Err(e) => panic_message(e.into_panic()),
    }
}

fn panic_message(panic: Box<dyn Any + Send>) -> String {
    match panic.downcast::<String>() {
        Ok(message) => *message,
        Err(panic) => panic.downcast::<&str>().unwrap().to_string(),
    }
}

/// Send a message, and wait for the server to receive it.
async fn send_received(stream: &mut GrpcResultsStream, results: &TestResults) {
    let expected = results.received().len() + 1;
    stream.send(&[1]).await;
    while results.received().len() < expected {
        tokio::time::sleep(Duration::from_millis(10)).await;
    }
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn messages_arrive_unchanged_and_finish_returns_succeeded() {
    let results = TestResults::new(StreamStatus::Succeeded);
    let server = TestServer::start(results.clone(), TestHealth::serving());

    // these are not valid protobuf, to check nothing is decoded or re-encoded.
    let messages: [&[u8]; 3] = [&[0xff, 0xfe, 0x00, 0x01], &[], &[0x0a; 1000]];

    let mut stream = connect(&server).await;
    for msg in messages {
        stream.send(msg).await;
    }

    assert_eq!(1, stream.finish().await);
    assert_eq!(Vec::from(messages), results.received());
    assert_eq!(Some(Ok(())), results.ended());
    assert_eq!(stream.stats.messages_sent, 3);
    assert_eq!(stream.stats.bytes_sent, 1004);

    stream.disconnect().await;
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn finish_returns_failed() {
    let server = TestServer::start(
        TestResults::new(StreamStatus::Failed),
        TestHealth::serving(),
    );

    let mut stream = connect(&server).await;
    stream.send(&[1, 2, 3]).await;

    assert_eq!(2, stream.finish().await);
    stream.disconnect().await;
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn connect_retries_until_serving() {
    let results = TestResults::new(StreamStatus::Succeeded);
    let health = TestHealth::new([
        ServingStatus::NotServing,
        ServingStatus::NotServing,
        ServingStatus::Serving,
    ]);
    let server = TestServer::start(results.clone(), health.clone());

    let mut stream = connect(&server).await;
    assert_eq!(3, health.checks());

    stream.send(&[1]).await;
    assert_eq!(1, stream.finish().await);
    assert_eq!(vec![Bytes::from_static(&[1])], results.received());
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn connect_panics_if_never_serving() {
    let health = TestHealth::new([ServingStatus::NotServing]);
    let server = TestServer::start(TestResults::new(StreamStatus::Succeeded), health.clone());
    let address = server.address.clone();

    let message = expect_panic(async move {
        GrpcResultsStream::connect(&address, RETRY_DELAY).await;
    })
    .await;

    assert!(
        message.contains("failed to connect to the result processor"),
        "{message}"
    );
    assert!(message.contains("NOT_SERVING"), "{message}");
    assert_eq!(MAX_RETRIES + 1, health.checks());
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn connect_panics_if_nothing_is_listening() {
    let mut server = TestServer::start(
        TestResults::new(StreamStatus::Succeeded),
        TestHealth::serving(),
    );
    server.kill();
    let address = server.address.clone();

    let message = expect_panic(async move {
        GrpcResultsStream::connect(&address, RETRY_DELAY).await;
    })
    .await;

    assert!(
        message.contains("failed to connect to the result processor"),
        "{message}"
    );
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(20000)]
async fn slow_server_blocks_sender() {
    const MESSAGE_SIZE: usize = 256 * 1024;
    const MESSAGES: usize = 400;

    let gate = Arc::new(Notify::new());
    let mut results = TestResults::new(StreamStatus::Succeeded);
    results.gate = Some(gate.clone());
    let server = TestServer::start(results.clone(), TestHealth::serving());

    let mut stream = connect(&server).await;
    let sent = Arc::new(AtomicUsize::new(0));
    let sender = tokio::spawn({
        let sent = sent.clone();
        async move {
            for i in 0..MESSAGES {
                stream.send(&[i as u8; MESSAGE_SIZE]).await;
                sent.fetch_add(1, SeqCst);
            }
            stream
        }
    });

    // the server is not reading, so sending stalls once the channel and HTTP/2 windows are full.
    tokio::time::sleep(Duration::from_millis(500)).await;
    let stalled_at = sent.load(SeqCst);
    tokio::time::sleep(Duration::from_millis(500)).await;
    assert_eq!(stalled_at, sent.load(SeqCst), "sending did not stall");
    assert!(
        stalled_at < MESSAGES / 4,
        "{stalled_at} messages were buffered"
    );
    assert!(!sender.is_finished());

    // once the server reads, everything is sent.
    gate.notify_one();
    let mut stream = sender.await.unwrap();
    assert_eq!(1, stream.finish().await);

    let received = results.received();
    assert_eq!(MESSAGES, received.len());
    for (i, batch) in received.iter().enumerate() {
        assert_eq!(&[i as u8; MESSAGE_SIZE][..], &batch[..]);
    }
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn send_panics_when_server_fails_the_stream() {
    let mut results = TestResults::new(StreamStatus::Succeeded);
    results.fail_after = Some(1);
    let server = TestServer::start(results, TestHealth::serving());

    let mut stream = connect(&server).await;
    let message = expect_panic(async move {
        loop {
            stream.send(&[1, 2, 3]).await;
        }
    })
    .await;

    assert!(message.contains("failed to stream message"), "{message}");
    assert!(message.contains(&server.address), "{message}");
    assert!(message.contains("going away"), "{message}");
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn send_panics_when_server_drops() {
    let results = TestResults::new(StreamStatus::Succeeded);
    let mut server = TestServer::start(results.clone(), TestHealth::serving());

    let mut stream = connect(&server).await;
    send_received(&mut stream, &results).await;

    server.kill();

    let message = expect_panic(async move {
        loop {
            stream.send(&[1, 2, 3]).await;
        }
    })
    .await;

    assert!(message.contains("failed to stream message"), "{message}");
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn finish_returns_error_when_server_drops() {
    let results = TestResults::new(StreamStatus::Succeeded);
    let mut server = TestServer::start(results.clone(), TestHealth::serving());

    let mut stream = connect(&server).await;
    send_received(&mut stream, &results).await;

    server.kill();

    assert_eq!(-1, stream.finish().await);
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn finish_twice_returns_error() {
    let server = TestServer::start(
        TestResults::new(StreamStatus::Succeeded),
        TestHealth::serving(),
    );

    let mut stream = connect(&server).await;
    assert_eq!(1, stream.finish().await);
    assert_eq!(-1, stream.finish().await);
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn send_after_finish_panics() {
    let server = TestServer::start(
        TestResults::new(StreamStatus::Succeeded),
        TestHealth::serving(),
    );

    let mut stream = connect(&server).await;
    assert_eq!(1, stream.finish().await);

    let message = expect_panic(async move { stream.send(&[1]).await }).await;
    assert!(message.contains("already been finished"), "{message}");
}

#[tokio::test(flavor = "multi_thread")]
#[timeout(10000)]
async fn disconnect_without_finish_cancels_the_stream() {
    let results = TestResults::new(StreamStatus::Succeeded);
    let server = TestServer::start(results.clone(), TestHealth::serving());

    let mut stream = connect(&server).await;
    send_received(&mut stream, &results).await;

    stream.disconnect().await;

    // the server must not see a half-close, which would mean the results are complete.
    while results.ended().is_none() {
        tokio::time::sleep(Duration::from_millis(10)).await;
    }
    assert_eq!(Some(Err(Code::Cancelled)), results.ended());
}
