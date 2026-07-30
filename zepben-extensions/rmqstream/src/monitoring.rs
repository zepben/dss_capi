use opentelemetry::global;
use opentelemetry_otlp::MetricExporter;
use opentelemetry_sdk::metrics::SdkMeterProvider;
use std::error::Error;
use std::sync::Mutex;
use tracing::{Level, debug, warn};
use tracing_log::LogTracer;
use tracing_subscriber::FmtSubscriber;

/// Has this library initialised logging yet?
pub static LOGGING_INITIALISED: Mutex<bool> = Mutex::new(false);

pub fn initialise_logging() {
    if let Err(e) = try_enable_logging::<DefaultLogging>() {
        warn!(
            "tracing or logging already initialised. it may have been initialised by another library: {e}"
        )
    }
}

pub fn try_enable_logging<L: LoggingProvider>() -> Result<(), Box<dyn Error>> {
    L::enable()
}

/// Initialise OpenTelemetry metrics over otlp-grpc. If monitoring is not enabled in the environment
/// then metrics will not be exported, and a noop implementation used.
pub fn initialise_metrics() {
    if std::env::var("ZEPBEN_OPENTELEMETRY_ENABLED") != Ok(String::from("1")) {
        debug!("opentelemetry disabled. no metrics will be emitted")
    }

    // this automatically picks up protocol and endpoint from environment variables.
    let exporter = match MetricExporter::builder().with_tonic().build() {
        Ok(provider) => provider,
        Err(e) => return warn!("failed to initialise metrics: {e}"),
    };
    let provider = SdkMeterProvider::builder()
        .with_periodic_exporter(exporter)
        .build();

    global::set_meter_provider(provider);
}

/// This allows us to abstract over enabling logging, and test that logging is
/// enabled
pub trait LoggingProvider {
    fn enable() -> Result<(), Box<dyn Error>>;
}

pub struct DefaultLogging;

impl LoggingProvider for DefaultLogging {
    fn enable() -> Result<(), Box<dyn Error>> {
        let initialised = *LOGGING_INITIALISED.lock()?;

        if !initialised {
            let subscriber = FmtSubscriber::builder()
                .with_max_level(Level::DEBUG)
                .finish();
            tracing::subscriber::set_global_default(subscriber)?;
            LogTracer::init()?;
            *LOGGING_INITIALISED.lock()? = true;
        }

        Ok(())
    }
}

#[cfg(test)]
pub struct MockLogging;

#[cfg(test)]
impl LoggingProvider for MockLogging {
    fn enable() -> Result<(), Box<dyn Error>> {
        if !*LOGGING_INITIALISED.lock()? {
            *LOGGING_INITIALISED.lock()? = true;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{error::Error, sync::Mutex};

    /// This allows us to synchronise our tests for tracing initialisation. We use
    /// this to make sure only one tracing initialisation test runs at a once.
    static SYNC_INITIALISE_TRACING: Mutex<()> = Mutex::new(());

    fn test_logging(test: impl Fn() -> Result<(), Box<dyn Error>>) -> Result<(), Box<dyn Error>> {
        let sync = SYNC_INITIALISE_TRACING.lock()?;
        reset()?;
        test()?;
        drop(sync);
        Ok(())
    }

    fn reset() -> Result<(), Box<dyn Error>> {
        *LOGGING_INITIALISED.lock()? = false;
        Ok(())
    }

    #[test]
    fn initialise_logging_sets_global_var() -> Result<(), Box<dyn Error>> {
        test_logging(|| {
            assert!(!*LOGGING_INITIALISED.lock()?);
            try_enable_logging::<MockLogging>()?;
            assert!(*LOGGING_INITIALISED.lock()?);
            Ok(())
        })
    }

    /// Test that `try_enable_logging` handles being called twice, without throwing
    /// errors
    #[test]
    fn initialise_logging_twice() -> Result<(), Box<dyn Error>> {
        test_logging(|| {
            assert!(!*LOGGING_INITIALISED.lock()?);
            try_enable_logging::<MockLogging>()?;
            assert!(*LOGGING_INITIALISED.lock()?);
            try_enable_logging::<MockLogging>()?;
            Ok(())
        })
    }
}
