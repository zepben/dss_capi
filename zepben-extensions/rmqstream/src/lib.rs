use std::ffi::CStr;
use std::slice;
use std::time::{Duration, Instant};
use rabbitmq_stream_client::{Environment, NoDedup, Producer};
use tracing::{debug, error, info, Level, trace, warn};
use lazy_static::lazy_static;
use rabbitmq_stream_client::error::ProducerCloseError;
use rabbitmq_stream_client::types::{Message, ResponseCode};
use tokio::runtime::Runtime;
use tokio::time::sleep;
use tracing_subscriber::FmtSubscriber;

lazy_static! {
    static ref RUNTIME: Runtime = Runtime::new().unwrap();
}
static mut PRODUCER: Option<Producer<NoDedup>> = None;
static mut BUSY_TIME: Duration = Duration::ZERO;
static mut START_TIME: Option<Instant> = None;
static mut TOTAL_MESSAGES: u32 = 0;
static mut TOTAL_BYTES: usize = 0;

#[no_mangle]
pub unsafe extern "C" fn init_tracing() {
    let subscriber = FmtSubscriber::builder()
        .with_max_level(Level::DEBUG)
        .finish();
    tracing::subscriber::set_global_default(subscriber).expect("setting default subscriber failed");
    info!("Rust tracing enabled at DEBUG level.");
}

#[no_mangle]
pub unsafe extern "C" fn connect_to_stream(
    _host: *const libc::c_char,
    _port: libc::c_int,
    _user: *const libc::c_char,
    _pass: *const libc::c_char,
    _stream: *const libc::c_char,
    _heartbeat: libc::c_int,
) {
    if PRODUCER.is_some() {
        info!("Already connected.");
        return;
    }

    debug!("Reading in parameters from C types...");
    let host = CStr::from_ptr(_host).to_string_lossy().to_string();
    let port = _port as u16;
    let user = CStr::from_ptr(_user).to_string_lossy().to_string();
    let pass = CStr::from_ptr(_pass).to_string_lossy().to_string();
    let stream = CStr::from_ptr(_stream).to_string_lossy().to_string();
    let heartbeat = _heartbeat as u32;

    debug!("C params read. Connecting to RabbitMQ stream ({user}@{host}:{port}, stream {stream})...");

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
                .build()
                .await
            {
                Ok(environment) => {
                    debug!("Connected. Making producer...");
                    match environment
                        .producer()
                        .batch_size(100000)
                        .batch_delay(Duration::from_millis(250))
                        .build(&stream)
                        .await
                    {
                        Ok(producer) => return producer,
                        Err(e) => {
                            last_error = Some(e.to_string());
                            warn!("Failed to create producer (attempt {} of {}): {}", retries + 1, max_retries, e);
                        }
                    }
                },
                Err(e) => {
                    last_error = Some(e.to_string());
                    warn!("Connection failed (attempt {} of {}): {}", retries + 1, max_retries, e);
                }
            }

            retries += 1;
            if retries < max_retries {
                sleep(Duration::from_secs(1)).await; // Wait before retrying
            }
        }

        panic!(
            "Could not connect to RabbitMQ after {} attempts. Last error: {:?}",
            max_retries,
            last_error
        );
    });

    PRODUCER = Some(producer);
    TOTAL_MESSAGES = 0;
    TOTAL_BYTES = 0;
    START_TIME = Some(Instant::now());
    info!(
        "Connected to RabbitMQ {}@{}:{}, for stream '{}'.",
        &user, &host, port, &stream
    );
}

#[no_mangle]
pub unsafe extern "C" fn disconnect_from_stream() {
    if let Some(producer) = PRODUCER.take() {
        RUNTIME.block_on(async {
            // There seems to be a bug in the rust stream client lib: outgoing messages are not
            // confirmed to have been sent before attempting to close the producer. We wait
            // 2 seconds here to be safe.
            sleep(Duration::from_secs(2)).await;
            producer
                .close()
                .await
                .map_err(|e| match e {
                    ProducerCloseError::Close { stream: _, status: ResponseCode::PublisherDoesNotExist } => {
                        // Results processor may have already deleted the stream (and consequently, the publisher)
                        // so we handle it gracefully here.
                        eprintln!("Publisher does not exist (has the stream been deleted?), but the producer has closed anyway.");
                    }
                    _ => panic!("Unexpected error when closing producer: {:?}", e)
                }).unwrap_or_else(|_| ())
        });
        let seconds_elapsed = START_TIME.unwrap().elapsed().as_secs_f64();
        let busy_percent = (BUSY_TIME.as_secs_f64() / seconds_elapsed) * 100.0;
        let msg_per_sec = TOTAL_MESSAGES as f64 / seconds_elapsed;
        let bits_per_sec = 8.0 * TOTAL_BYTES as f64 / seconds_elapsed;
        info!("Disconnected from RabbitMQ. {} total messages, {}% busy, {} msg/sec, {} bits/sec", 
            TOTAL_MESSAGES, busy_percent, msg_per_sec, bits_per_sec);
    } else {
        info!("Already disconnected.");
    }
}

#[no_mangle]
pub unsafe extern "C" fn stream_out_message(msg_ptr: *const libc::c_void, msg_len: libc::size_t) {
    if let Some(producer) = &mut PRODUCER {
        let busy_start = Instant::now();
        let msg_u8_ptr = msg_ptr as *const u8;
        let msg_bytes = slice::from_raw_parts(msg_u8_ptr, msg_len).to_vec();
        RUNTIME.block_on(async move {
            producer
                .send(
                    Message::builder().body(msg_bytes).build(),
                    move |res| async move {
                        match res {
                            Ok(_) => {
                                trace!("Streamed a message containing {} bytes.", msg_len)
                            }
                            Err(e) => {
                                error!("Could not confirm message! Full error: {}", e)
                            }
                        }
                    },
                )
                .await
                .expect("Could not send message!");
        });
        BUSY_TIME += busy_start.elapsed();
        TOTAL_MESSAGES += 1;
        TOTAL_BYTES += msg_len;
    } else {
        error!("Not connected to a RabbitMQ stream!");
    }
}
