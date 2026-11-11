use rabbitmq_stream_client::{NoDedup, Producer};
use std::ffi::CStr;
use std::slice;
use std::sync::{LazyLock, Mutex};
use std::time::Duration;
use tokio::runtime::Runtime;
use tracing::{debug, error, info};

use crate::grpc_stream::GrpcResultsStream;
use crate::monitoring::initialise_logging;
use crate::results_stream::ResultsStream;

pub(crate) mod grpc_stream;
pub(crate) mod monitoring;
pub(crate) mod producer;
pub(crate) mod results_stream;
pub(crate) mod retry;
pub(crate) mod stats;

/// This will be initialised using this closure on first use
static RUNTIME: LazyLock<Runtime> = LazyLock::new(|| Runtime::new().unwrap());

/// The transport that results are streamed over.
enum ResultsTransport {
    RabbitMq(ResultsStream<Producer<NoDedup>>),
    Grpc(GrpcResultsStream),
}

static RESULTS_STREAM: Mutex<Option<ResultsTransport>> = Mutex::new(None);

/// The timeout when waiting for inflight messages.
const INFLIGHT_TIMEOUT: Duration = Duration::from_secs(5);

#[unsafe(no_mangle)]
pub extern "C" fn init_tracing() {
    initialise_logging();
}

#[unsafe(no_mangle)]
#[allow(clippy::missing_safety_doc)]
pub unsafe extern "C" fn connect_to_stream(
    _host: *const libc::c_char,
    _port: libc::c_int,
    _user: *const libc::c_char,
    _pass: *const libc::c_char,
    _stream: *const libc::c_char,
    _heartbeat: libc::c_int,
) {
    initialise_logging();

    if RESULTS_STREAM.lock().unwrap().is_some() {
        info!("Already connected.");
        return;
    }

    debug!("Reading in parameters from C types...");
    let host = unsafe { CStr::from_ptr(_host).to_string_lossy().to_string() };
    let port = _port as u16;
    let username = unsafe { CStr::from_ptr(_user).to_string_lossy().to_string() };
    let password = unsafe { CStr::from_ptr(_pass).to_string_lossy().to_string() };
    let stream_name = unsafe { CStr::from_ptr(_stream).to_string_lossy().to_string() };
    let heartbeat = _heartbeat as u32;
    debug!("C params read");

    *RESULTS_STREAM.lock().unwrap() = RUNTIME.block_on(async {
        Some(ResultsTransport::RabbitMq(
            ResultsStream::new(&host, port, &username, &password, &stream_name, heartbeat).await,
        ))
    });
}

/// Connect to the result processor's gRPC results stream at `address` (`host:port`).
#[unsafe(no_mangle)]
#[allow(clippy::missing_safety_doc)]
pub unsafe extern "C" fn connect_to_grpc_stream(_address: *const libc::c_char) {
    initialise_logging();

    if RESULTS_STREAM.lock().unwrap().is_some() {
        info!("Already connected.");
        return;
    }

    let address = unsafe { CStr::from_ptr(_address).to_string_lossy().to_string() };

    let results_stream = run_blocking(GrpcResultsStream::new(&address));
    *RESULTS_STREAM.lock().unwrap() = Some(ResultsTransport::Grpc(results_stream));
}

/// Half-close the gRPC results stream, and wait for the result processor's summary. Returns the
/// `StreamStatus` value of the summary, or -1 if the stream failed or is not connected.
#[unsafe(no_mangle)]
pub extern "C" fn finish_grpc_stream() -> libc::c_int {
    initialise_logging();

    match RESULTS_STREAM.lock().unwrap().as_mut() {
        Some(ResultsTransport::Grpc(results_stream)) => run_blocking(results_stream.finish()),
        _ => {
            error!("not connected to a gRPC results stream!");
            -1
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn disconnect_from_stream() {
    initialise_logging();

    match RESULTS_STREAM.lock().unwrap().take() {
        Some(ResultsTransport::RabbitMq(mut results_stream)) => {
            run_blocking(async {
                let _ = results_stream.wait_no_inflight(INFLIGHT_TIMEOUT).await;
                results_stream.disconnect().await;
            });
        }
        Some(ResultsTransport::Grpc(results_stream)) => {
            run_blocking(results_stream.disconnect());
        }
        None => info!("Already disconnected."),
    }
}

#[unsafe(no_mangle)]
#[allow(clippy::missing_safety_doc)]
pub unsafe extern "C" fn stream_out_message(
    msg_ptr: *const libc::c_void,
    msg_len: libc::size_t,
    confirm: bool,
) {
    match RESULTS_STREAM.lock().unwrap().as_mut() {
        Some(ResultsTransport::RabbitMq(results_stream)) => {
            let msg = unsafe { slice::from_raw_parts(msg_ptr as *const u8, msg_len) };
            run_blocking(async {
                match confirm {
                    true => results_stream.send_and_wait_confirmation(msg).await,
                    false => results_stream.send(msg).await,
                }
            })
        }
        // `confirm` is ignored, as gRPC flow control blocks when the result processor is behind.
        Some(ResultsTransport::Grpc(results_stream)) => {
            let msg = unsafe { slice::from_raw_parts(msg_ptr as *const u8, msg_len) };
            run_blocking(results_stream.send(msg))
        }
        None => error!("not connected to a results stream!"),
    }
}

fn run_blocking<F: Future>(future: F) -> F::Output {
    RUNTIME.block_on(future)
}
