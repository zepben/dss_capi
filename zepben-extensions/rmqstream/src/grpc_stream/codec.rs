//! A pass-through gRPC codec for the `OpenDssResultsStream` service.
//!
//! `OpenDssReportBatch` messages are serialised by protobuf-c on the C side. Rather than decoding
//! them and re-encoding them with prost, the serialised bytes are written to the stream unchanged
//! as an [EncodedBatch]. Other messages (i.e. `StreamSummary`) are encoded and decoded with prost.

use bytes::{Buf, BufMut, Bytes};
use prost::Message;
use std::marker::PhantomData;
use tonic::{
    Status,
    codec::{Codec, DecodeBuf, Decoder, EncodeBuf, Encoder},
};

use crate::grpc_stream::proto::StreamSummary;

/// A serialised `OpenDssReportBatch`, which is sent over the stream as-is.
#[derive(Clone, Debug, PartialEq)]
pub struct EncodedBatch(pub Bytes);

/// A message that can be sent or received with the [PassThroughCodec].
pub trait CodecMessage: Sized + Send + 'static {
    fn encode_to(self, buf: &mut EncodeBuf<'_>) -> Result<(), Status>;

    fn decode_from(buf: &mut DecodeBuf<'_>) -> Result<Self, Status>;
}

impl CodecMessage for EncodedBatch {
    fn encode_to(self, buf: &mut EncodeBuf<'_>) -> Result<(), Status> {
        buf.put_slice(&self.0);
        Ok(())
    }

    fn decode_from(buf: &mut DecodeBuf<'_>) -> Result<Self, Status> {
        Ok(EncodedBatch(buf.copy_to_bytes(buf.remaining())))
    }
}

impl CodecMessage for StreamSummary {
    fn encode_to(self, buf: &mut EncodeBuf<'_>) -> Result<(), Status> {
        self.encode(buf)
            .map_err(|e| Status::internal(format!("failed to encode StreamSummary: {e}")))
    }

    fn decode_from(buf: &mut DecodeBuf<'_>) -> Result<Self, Status> {
        StreamSummary::decode(buf)
            .map_err(|e| Status::internal(format!("failed to decode StreamSummary: {e}")))
    }
}

/// A codec that encodes `E` and decodes `D`. This is used by the generated client and server code.
pub struct PassThroughCodec<E, D>(PhantomData<(E, D)>);

impl<E, D> Default for PassThroughCodec<E, D> {
    fn default() -> Self {
        Self(PhantomData)
    }
}

impl<E: CodecMessage, D: CodecMessage> Codec for PassThroughCodec<E, D> {
    type Encode = E;
    type Decode = D;
    type Encoder = PassThroughEncoder<E>;
    type Decoder = PassThroughDecoder<D>;

    fn encoder(&mut self) -> Self::Encoder {
        PassThroughEncoder(PhantomData)
    }

    fn decoder(&mut self) -> Self::Decoder {
        PassThroughDecoder(PhantomData)
    }
}

pub struct PassThroughEncoder<E>(PhantomData<E>);

impl<E: CodecMessage> Encoder for PassThroughEncoder<E> {
    type Item = E;
    type Error = Status;

    fn encode(&mut self, item: Self::Item, dst: &mut EncodeBuf<'_>) -> Result<(), Self::Error> {
        item.encode_to(dst)
    }
}

pub struct PassThroughDecoder<D>(PhantomData<D>);

impl<D: CodecMessage> Decoder for PassThroughDecoder<D> {
    type Item = D;
    type Error = Status;

    fn decode(&mut self, src: &mut DecodeBuf<'_>) -> Result<Option<Self::Item>, Self::Error> {
        D::decode_from(src).map(Some)
    }
}
