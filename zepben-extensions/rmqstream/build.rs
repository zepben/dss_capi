//! Generate the gRPC client (and the server used in tests) for the `OpenDssResultsStream` service.
//!
//! The protos in `proto/` are vendored from the Zepben protobuf repository (`ewb-grpc`), and must
//! be kept in sync with it. Only `OpenDssResultsStream.proto` is generated; the other files are
//! only needed to resolve its imports. `protoc` is provided by `protoc-bin-vendored`, so the build
//! does not need a system `protoc`.
//!
//! `OpenDssReportBatch` (and the rest of the report messages) are not generated. Batches are
//! serialised by protobuf-c on the C side, and are passed through to the stream unchanged as an
//! `EncodedBatch`, using a pass-through codec.

use std::path::PathBuf;

const PROTO_DIR: &str = "proto";
const RESULTS_STREAM_PROTO: &str = "zepben/protobuf/hc/opendss/OpenDssResultsStream.proto";

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut config = tonic_prost_build::Config::new();
    config.protoc_executable(protoc_bin_vendored::protoc_bin_path()?);
    let protoc_include = protoc_bin_vendored::include_path()?;

    println!("cargo:rerun-if-changed={PROTO_DIR}");

    // prost generates code for every file in the descriptor set, including imports. Only keep the
    // results stream file, so the report messages (only ever passed through) are not generated.
    let mut fds = config.load_fds(
        &[PathBuf::from(PROTO_DIR).join(RESULTS_STREAM_PROTO)],
        &[PathBuf::from(PROTO_DIR), protoc_include],
    )?;
    fds.file.retain(|file| file.name() == RESULTS_STREAM_PROTO);

    tonic_prost_build::configure()
        .build_client(true)
        .build_server(true)
        .codec_path("crate::grpc_stream::codec::PassThroughCodec")
        .extern_path(
            ".zepben.protobuf.hc.opendss.OpenDssReportBatch",
            "crate::grpc_stream::codec::EncodedBatch",
        )
        .compile_fds_with_config(fds, config)?;

    Ok(())
}
