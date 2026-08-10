# Repository Guidelines

## Project Structure & Module Organization

This repository contains the `rmqstream` Rust crate, built as a statically linked library for C interoperability. `src/lib.rs` defines the exported C ABI and shared Tokio runtime. RabbitMQ producer abstraction and stream lifecycle logic live in `src/producer.rs` and `src/results_stream.rs`; retry behavior, telemetry setup, and counters are in `src/retry.rs`, `src/monitoring.rs`, and `src/stats.rs`. Unit tests are colocated with their modules under `#[cfg(test)]`; the larger results-stream suite is in `src/results_stream/tests.rs`. Build artifacts belong in `target/` and must not be committed.

## Build, Test, and Development Commands

- `cargo build` compiles the debug static library.
- `cargo build --release` produces an optimized library under `target/release/` for downstream C consumers.
- `cargo test` runs all unit and asynchronous Tokio tests.
- `cargo fmt --all -- --check` verifies standard Rust formatting; use `cargo fmt --all` to apply it.
- `cargo clippy --all-targets --all-features -- -D warnings` runs lint checks and treats warnings as failures.

Run formatting, Clippy, and tests before opening a pull request.

## Coding Style & Naming Conventions

Use rustfmt defaults (four-space indentation) and idiomatic Rust naming: `snake_case` for functions, modules, and variables; `CamelCase` for types and traits; `SCREAMING_SNAKE_CASE` for constants. Keep unsafe FFI operations narrowly scoped, document pointer and ownership expectations, and preserve exported symbols with `#[unsafe(no_mangle)]`. Prefer structured `tracing` calls over direct printing. Add rustdoc comments for public APIs and for non-obvious concurrency or retry behavior.

## Testing Guidelines

Use built-in Rust tests, `#[tokio::test]` for async behavior, and `ntest::timeout` where a hang would obscure a failure. Name tests after observable behavior, for example `wait_confirmation_times_out`. Keep test doubles close to the module they exercise. Tests should cover success, retry/error, timeout, and confirmation-state transitions; no numeric coverage threshold is currently configured.

## Commit & Pull Request Guidelines

Follow the existing history: write concise, imperative, sentence-case subjects such as `Fix updating confirmation status when new messages are sent`. Keep each commit focused. Pull requests should explain the behavior change, identify C ABI or RabbitMQ compatibility impacts, link the relevant issue, and include the commands run. Add logs or metric examples when observability changes; screenshots are generally unnecessary for this library.

## Configuration & Security

Never commit RabbitMQ credentials or telemetry endpoints. OpenTelemetry export is enabled with `ZEPBEN_OPENTELEMETRY_ENABLED=1`; configure OTLP settings through environment variables. Avoid logging passwords or message payloads, especially across the C boundary.
