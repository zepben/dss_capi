use rabbitmq_stream_client::error::ProducerCloseError;
use rabbitmq_stream_client::error::ProducerPublishError;
use rabbitmq_stream_client::types::{Message, ResponseCode};
use rabbitmq_stream_client::{Environment, NoDedup, Producer};
use rand::random_range;
use std::ffi::CStr;
use std::slice;
use std::sync::{LazyLock, Mutex};
use std::time::{Duration, Instant};
use tokio::runtime::Runtime;
use tokio::time::sleep;
use tracing::{debug, error, info, trace, warn};

use crate::logging::initialise_logging;

pub(crate) mod logging;

/// This will be initialised using this closure on first use
static RUNTIME: LazyLock<Runtime> = LazyLock::new(|| Runtime::new().unwrap());

static PRODUCER: Mutex<Option<Producer<NoDedup>>> = Mutex::new(None);
static STATS: Mutex<Stats> = Mutex::new(Stats::new());

struct Stats {
    pub busy_time: Duration,
    pub start_time: Option<Instant>,
    pub total_messages: u32,
    pub total_bytes: usize,
}

impl Stats {
    const fn new() -> Self {
        Self {
            busy_time: Duration::ZERO,
            start_time: None,
            total_messages: 0,
            total_bytes: 0,
        }
    }
}

#[unsafe(no_mangle)]
#[allow(clippy::missing_safety_doc)]
pub unsafe extern "C" fn init_tracing() {
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

    if PRODUCER.lock().unwrap().is_some() {
        info!("Already connected.");
        return;
    }

    debug!("Reading in parameters from C types...");
    let host = unsafe { CStr::from_ptr(_host).to_string_lossy().to_string() };
    let port = _port as u16;
    let user = unsafe { CStr::from_ptr(_user).to_string_lossy().to_string() };
    let pass = unsafe { CStr::from_ptr(_pass).to_string_lossy().to_string() };
    let stream = unsafe { CStr::from_ptr(_stream).to_string_lossy().to_string() };
    let heartbeat = _heartbeat as u32;

    debug!(
        "C params read. Connecting to RabbitMQ stream ({user}@{host}:{port}, stream {stream})..."
    );

    let producer = RUNTIME.block_on(async {
        // Retry connection up to 3 times
        let mut retries = 0;
        let max_retries = 3;
        let mut last_error = None;

        while retries < max_retries {
            match Environment::builder()
                .host(&host)
                .port(port)
                .heartbeat(heartbeat)
                .username(&user)
                .password(&pass)
                .load_balancer_mode(true)
                .build()
                .await
            {
                Ok(environment) => {
                    debug!("Connected. Making producer...");
                    match environment
                        .producer()
                        .batch_size(100000)
                        .build(&stream)
                        .await
                    {
                        Ok(producer) => return producer,
                        Err(e) => {
                            last_error = Some(e.to_string());
                            warn!(
                                "Failed to create producer (attempt {} of {}): {}",
                                retries + 1,
                                max_retries,
                                e
                            );
                        }
                    }
                }
                Err(e) => {
                    last_error = Some(e.to_string());
                    warn!(
                        "Connection failed (attempt {} of {}): {}",
                        retries + 1,
                        max_retries,
                        e
                    );
                }
            }

            retries += 1;
            if retries < max_retries {
                sleep(Duration::from_secs(1)).await; // Wait before retrying
            }
        }

        panic!(
            "Could not connect to RabbitMQ after {} attempts. Last error: {:?}",
            max_retries, last_error
        );
    });

    *PRODUCER.lock().unwrap() = Some(producer);
    STATS.lock().unwrap().total_messages = 0;
    STATS.lock().unwrap().total_bytes = 0;
    STATS.lock().unwrap().start_time = Some(Instant::now());

    info!("Connected to RabbitMQ {user}@{host}:{port}, for stream '{stream}'.");
}

#[unsafe(no_mangle)]
#[allow(clippy::missing_safety_doc)]
pub unsafe extern "C" fn disconnect_from_stream() {
    initialise_logging();

    match PRODUCER.lock().unwrap().take() {
        Some(producer) => {
            RUNTIME.block_on(async {
                match producer.close().await {
                    Err(ProducerCloseError::Close { status: ResponseCode::PublisherDoesNotExist, .. }) => {
                        // Results processor may have already deleted the stream (and consequently, the publisher)
                        // so we handle it gracefully here.
                        warn!("Publisher does not exist (has the stream been deleted?), but the producer has closed anyway.");
                    }
                    Err(e) => panic!("Unexpected error when closing producer: {:?}", e),
                    Ok(_) => (),
                }
            });
            let total_messages = STATS.lock().unwrap().total_messages;
            let seconds_elapsed = STATS
                .lock()
                .unwrap()
                .start_time
                .unwrap()
                .elapsed()
                .as_secs_f64();
            let busy_percent =
                (STATS.lock().unwrap().busy_time.as_secs_f64() / seconds_elapsed) * 100.0;
            let msg_per_sec = total_messages as f64 / seconds_elapsed;
            let bits_per_sec = 8.0 * STATS.lock().unwrap().total_bytes as f64 / seconds_elapsed;
            info!(
                "Disconnected from RabbitMQ. {total_messages} total messages, {busy_percent}% busy, {msg_per_sec} msg/sec, {bits_per_sec} bits/sec",
            );
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
    if let Some(producer) = &mut *PRODUCER.lock().unwrap() {
        let busy_start = Instant::now();
        let msg_u8_ptr = msg_ptr as *const u8;
        let msg_bytes = unsafe { slice::from_raw_parts(msg_u8_ptr, msg_len).to_vec() };
        RUNTIME.block_on(async move {
            let message: Message = Message::builder().body(msg_bytes).build();

            if confirm {
                try_send(message, |message| async {
                    let result = producer.send_with_confirm(message).await;
                    result.map(|_| ()) // replace the confirmation status with
                })
                .await;
            } else {
                // the callback `cb` passed to `send` is invoked on the confirmation of the message send.
                // since we dont need the confirmation callback for anything, we pass a noop closure

                try_send(message, |message| producer.send(message, |_| async {})).await;
            }
        });
        STATS.lock().unwrap().busy_time += busy_start.elapsed();
        STATS.lock().unwrap().total_messages += 1;
        STATS.lock().unwrap().total_bytes += msg_len;
    } else {
        error!("Not connected to a RabbitMQ stream!");
    }
}

const SEND_MAX_RETRIES: usize = 3;
const SEND_BACKOFF_DELAY: Duration = Duration::from_secs(5);

/// Try to send a message with the given `send` closure. If the `send` closure
/// returns `ProducerPublishError::Timeout` then the send is retried.
///
/// The message will be retried with exponential backoff
async fn try_send(
    message: Message,
    send: impl AsyncFn(Message) -> Result<(), ProducerPublishError>,
) {
    let mut delay: Duration = SEND_BACKOFF_DELAY;
    let mut retires = 0;

    while retires < SEND_MAX_RETRIES {
        match send(message.clone()).await {
            Ok(_) => trace!(
                "Streamed a message containing {} bytes",
                message.data().map_or(0, |data| data.len())
            ),
            Err(ProducerPublishError::Timeout) => {
                let actual_delay = jittered_delay(delay);

                warn!(
                    "Timeout publishing message. Waiting {}ms before retrying",
                    actual_delay.as_millis()
                );

                sleep(actual_delay).await;
                delay *= 2;
                retires += 1;
            }
            Err(e) => panic!("Could not send message: {e}"),
        }
    }
}

/// Return the given delay, with 50% random jitter applied
fn jittered_delay(delay: Duration) -> Duration {
    random_range(Duration::ZERO..delay) - (delay / 2)
}
