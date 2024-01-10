use std::ffi::CStr;
use std::slice;
use std::time::{Duration, Instant};
use rabbitmq_stream_client::{Environment, NoDedup, Producer};
use tracing::{debug, error, info, Level, trace};
use lazy_static::lazy_static;
use rabbitmq_stream_client::types::Message;
use tokio::runtime::Runtime;
use tracing_subscriber::FmtSubscriber;

lazy_static! {
    static ref RUNTIME: Runtime = Runtime::new().unwrap();
}
static mut PRODUCER: Option<Box<Producer<NoDedup>>> = None;
static mut BUSY_TIME: Duration = Duration::ZERO;
static mut START_TIME: Instant = Instant::now();
static mut TOTAL_MESSAGES: u32 = 0;

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
        let environment = Environment::builder()
            .host(&host)
            .port(port)
            .heartbeat(heartbeat)
            .username(&user)
            .password(&pass)
            .build()
            .await
            .expect("Could not connect to RabbitMQ, please check socket and credentials");
        debug!("Connected. Making producer...");
        environment
            .producer()
            .batch_size(10000)
            .batch_delay(Duration::from_millis(250))
            .build(&stream)
            .await
            .expect("Could not make producer. Does the stream exist?")
    });

    PRODUCER = Some(Box::new(producer));
    TOTAL_MESSAGES = 0;
    START_TIME = Instant::now();
    info!(
        "Connected to RabbitMQ {}@{}:{}, for stream '{}'.",
        &user, &host, port, &stream
    );
}

#[no_mangle]
pub unsafe extern "C" fn disconnect_from_stream() {
    if let Some(producer) = &mut PRODUCER {
        RUNTIME.block_on(async {
            producer
                .clone()
                .close()
                .await
                .expect("Could not close producer.");
        });
        PRODUCER = None;
        let busy_percent = (BUSY_TIME.as_secs_f64() / START_TIME.elapsed().as_secs_f64()) * 100;
        let msg_per_sec = TOTAL_MESSAGES as f64 / START_TIME.elapsed().as_secs_f64();
        info!("Disconnected from RabbitMQ. {} total messages, {}% busy, {} msg/sec", TOTAL_MESSAGES, busy_percent, msg_per_sec)
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
    } else {
        error!("Not connected to a RabbitMQ stream!");
    }
}