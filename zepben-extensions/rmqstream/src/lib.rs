use std::ffi::CStr;
use std::slice;
use std::sync::{LazyLock, Mutex};
use std::time::Duration;
use tokio::runtime::Runtime;
use tracing::{debug, error, info};

use crate::logging::initialise_logging;
use crate::results_stream::ResultsStream;

pub(crate) mod logging;
pub(crate) mod results_stream;
pub(crate) mod retry;
pub(crate) mod stats;

#[cfg(test)]
mod tests;

/// This will be initialised using this closure on first use
static RUNTIME: LazyLock<Runtime> = LazyLock::new(|| Runtime::new().unwrap());

static RESULTS_STREAM: Mutex<Option<ResultsStream>> = Mutex::new(None);

/// The timeout when waiting for all confirmations when disconnecting from the results stream
const CONFIRMATION_TIMEOUT: Duration = Duration::from_secs(5);

#[unsafe(no_mangle)]
#[allow(clippy::missing_safety_doc)]
pub extern "C" fn init_tracing() {
    initialise_logging();
}

#[unsafe(no_mangle)]
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
        Some(ResultsStream::new(&host, port, &username, &password, &stream_name, heartbeat).await)
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn disconnect_from_stream() {
    initialise_logging();

    match RESULTS_STREAM.lock().unwrap().take() {
        Some(mut results_stream) => {
            run_blocking(async {
                results_stream.wait_confirmation(CONFIRMATION_TIMEOUT).await;
                results_stream.disconnect().await;
            });
        }
        None => info!("Already disconnected."),
    }
}

#[unsafe(no_mangle)]
#[allow(clippy::missing_safety_doc)]
pub unsafe extern "C" fn stream_out_message(
    msg_ptr: *const libc::c_void,
    msg_len: libc::size_t,
    _confirm: bool,
) {
    // TODO: include some mechanism to log that confirms being disabled are not supported.

    if let Some(results_stream) = RESULTS_STREAM.lock().unwrap().as_mut() {
        let msg = unsafe { slice::from_raw_parts(msg_ptr as *const u8, msg_len) };
        run_blocking(async { results_stream.send(msg).await })
    } else {
        error!("not connected to a RabbitMQ stream!");
    }
}

fn run_blocking<F: Future>(future: F) -> F::Output {
    RUNTIME.block_on(future)
}
