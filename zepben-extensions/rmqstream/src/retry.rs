use rand::random_range;
use std::{fmt::Debug, time::Duration};
use tokio::time::sleep;
use tracing::warn;

pub(crate) const MAX_RETRIES: usize = 3;
pub(crate) const BACKOFF_DELAY: Duration = Duration::from_secs(5);

/// Try to run `f` with retries. `f` will be retried if the maximum number of
/// retries `MAX_RETRIES` has not been reached, and the predicate `retry_on` is
/// satisfied.
///
/// The `action` is used to provide a meaningful log message of what was being
/// tried when `f` is being retried.
pub async fn with_retries<T, E: Debug, F: Future<Output = Result<T, E>>>(
    action: &'static str,
    retry_on: impl Fn(&E) -> bool,
    on_retry: impl Fn(),
    f: impl Fn() -> F,
) -> Result<T, E> {
    let mut retires = 0;

    loop {
        match f().await {
            Err(e) if retry_on(&e) && retires < MAX_RETRIES => {
                let delay = jittered_delay(backoff(BACKOFF_DELAY, retires));
                warn!(
                    "{action} failed with error `{e:?}`, retrying in {}ms",
                    delay.as_millis()
                );

                sleep(delay).await;
                on_retry();
                retires += 1;
            }
            x => return x,
        }
    }
}

/// Calculate the delay between retires, given the initial delay, and the number
/// of retires already made.
fn backoff(initial_delay: Duration, mut retires: usize) -> Duration {
    let mut delay = initial_delay;
    while retires > 0 {
        delay *= 2;
        retires -= 1;
    }
    delay
}

/// Return the given delay, with 50% random jitter applied
fn jittered_delay(delay: Duration) -> Duration {
    delay - random_range(Duration::ZERO..(delay / 2))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::retry::{MAX_RETRIES, with_retries};

    use ntest::timeout;
    use rabbitmq_stream_client::error::ProducerPublishError;
    use tokio::sync::Mutex;

    #[tokio::test]
    #[timeout(100)]
    async fn with_retires_succeeds() -> Result<(), ()> {
        with_retries("test", |_: &()| true, || {}, async || Ok(())).await?;
        Ok(())
    }

    #[tokio::test(start_paused = true)]
    #[timeout(100)]
    async fn with_retires_is_retried() -> Result<(), ProducerPublishError> {
        // count the number of times our send function is called
        let tries = Mutex::new(0);

        let result = with_retries(
            "intentional",
            |_| true,
            || {},
            async || {
                *tries.lock().await += 1;
                Err::<(), _>("hello world")
            },
        )
        .await;

        assert_eq!(MAX_RETRIES + 1, *tries.lock().await);
        assert_eq!(Err("hello world"), result);
        Ok(())
    }

    #[tokio::test(start_paused = true)]
    #[timeout(100)]
    async fn with_retires_is_not_retried_if_error_doesnt_satisfy_predicate() {
        // count the number of times our send function is called
        let tries = Mutex::new(0);

        let result = with_retries(
            "intentional",
            |_| false,
            || {},
            async || {
                *tries.lock().await += 1;
                Err::<(), _>(String::from("an error"))
            },
        )
        .await;

        assert_eq!(1, *tries.lock().await);
        assert_eq!(Err(String::from("an error")), result)
    }

    #[tokio::test(start_paused = true)]
    async fn on_retry_is_called() {
        let retries = std::sync::Mutex::new(0);

        let _ = with_retries(
            "intentional",
            |_| true,
            || *retries.lock().unwrap() += 1,
            async || Err::<(), _>(String::from("an error")),
        )
        .await;

        assert_eq!(3, *retries.lock().unwrap());
    }

    #[test]
    fn jittered_delay_in_range() {
        let initial_delay = Duration::from_secs(1);
        let expected_range = (initial_delay / 2)..(initial_delay + initial_delay / 2);

        for _ in 0..1000 {
            assert!(expected_range.contains(&jittered_delay(initial_delay)))
        }
    }
}
