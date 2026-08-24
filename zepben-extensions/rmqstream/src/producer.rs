use rabbitmq_stream_client::{
    NoDedup, Producer,
    error::{ProducerCloseError, ProducerPublishError},
    types::Message,
};
use std::{future::Future, pin::Pin};

pub(crate) type ConfirmationResult = Result<bool, ProducerPublishError>;
pub(crate) type ConfirmationFuture = Pin<Box<dyn Future<Output = ()> + Send + Sync + 'static>>;
pub(crate) type ConfirmationCallback =
    Box<dyn FnOnce(ConfirmationResult) -> ConfirmationFuture + Send + Sync + 'static>;

/// The producer used by a results stream.
///
/// This trait allows testing of the [ResultsStream].
pub(crate) trait StreamProducer: Send + Sync {
    async fn send(
        &self,
        message: Message,
        on_confirmation: ConfirmationCallback,
    ) -> Result<(), ProducerPublishError>;

    async fn send_and_wait_confirmation(&self, message: Message) -> ConfirmationResult;

    async fn close(self) -> Result<(), ProducerCloseError>;
}

impl StreamProducer for Producer<NoDedup> {
    async fn send(
        &self,
        message: Message,
        on_confirmation: ConfirmationCallback,
    ) -> Result<(), ProducerPublishError> {
        Producer::<NoDedup>::send(self, message, move |result| {
            on_confirmation(result.map(|status| status.confirmed()))
        })
        .await
    }

    async fn send_and_wait_confirmation(&self, message: Message) -> ConfirmationResult {
        Producer::<NoDedup>::send_with_confirm(self, message)
            .await
            .map(|status| status.confirmed())
    }

    async fn close(self) -> Result<(), ProducerCloseError> {
        self.close().await
    }
}
