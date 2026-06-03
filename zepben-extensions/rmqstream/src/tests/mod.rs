use super::*;
use ntest::timeout;

#[tokio::test]
#[timeout(100)]
async fn try_send_succeeds() {
    // setup our fake `send` function
    let send = |_| async { Ok(()) };
    let message = Message::builder().body("deadbeef").build();

    try_send(message, send).await;
}

#[tokio::test(start_paused = true)]
#[timeout(100)]
async fn try_send_timeout() {
    // count the number of times our send function is called
    let retires = Mutex::new(0);

    // setup our fake `send` function
    let send = |_| async {
        *retires.lock().unwrap() += 1;
        Err(ProducerPublishError::Timeout)
    };

    let message = Message::builder().body("deadbeef").build();
    try_send(message, send).await;

    assert_eq!(SEND_MAX_RETRIES, *retires.lock().unwrap());
}

#[tokio::test]
#[should_panic]
#[timeout(100)]
async fn try_send_error() {
    // setup our fake `send` function
    let send = |_| async { Err(ProducerPublishError::Closed) };
    let message = Message::builder().body("deadbeef").build();

    try_send(message, send).await;
}

#[test]
fn jittered_delay_in_range() {
    let initial_delay = Duration::from_secs(1);
    let expected_range = (initial_delay / 2)..(initial_delay + initial_delay / 2);

    for _ in 0..1000 {
        assert!(expected_range.contains(&jittered_delay(initial_delay)))
    }
}
