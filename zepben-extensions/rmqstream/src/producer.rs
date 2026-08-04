use async_trait::async_trait;
use rabbitmq_stream_client::{
    NoDedup, Producer,
    error::{ProducerCloseError, ProducerPublishError},
    types::Message,
};
use std::{future::Future, pin::Pin};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum Confirmation {
    Confirmed,
    Unconfirmed,
}

pub(crate) type ConfirmationResult = Result<Confirmation, ProducerPublishError>;
pub(crate) type ConfirmationFuture = Pin<Box<dyn Future<Output = ()> + Send + Sync + 'static>>;
pub(crate) type ConfirmationCallback =
    Box<dyn FnOnce(ConfirmationResult) -> ConfirmationFuture + Send + Sync + 'static>;

/// The producer used by a results stream.
///
/// This trait allows testing of the [ResultsStream].
#[async_trait]
pub(crate) trait StreamProducer: Send + Sync {
    async fn send(
        &self,
        message: &Message,
        on_confirmation: ConfirmationCallback,
    ) -> Result<(), ProducerPublishError>;

    async fn close(self) -> Result<(), ProducerCloseError>;
}

#[async_trait]
impl StreamProducer for Producer<NoDedup> {
    async fn send(
        &self,
        message: &Message,
        on_confirmation: ConfirmationCallback,
    ) -> Result<(), ProducerPublishError> {
        Producer::<NoDedup>::send(self, message.clone(), move |result| {
            on_confirmation(result.map(|status| {
                if status.confirmed() {
                    Confirmation::Confirmed
                } else {
                    Confirmation::Unconfirmed
                }
            }))
        })
        .await
    }

    async fn close(self) -> Result<(), ProducerCloseError> {
        self.close().await
    }
}
