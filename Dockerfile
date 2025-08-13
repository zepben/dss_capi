FROM docker.io/library/amazoncorretto:11-al2023 AS java

FROM ghcr.io/zepben/dss-capi-builder:latest-rust AS builder

RUN mkdir -p /app /outputs/lib /outputs/include
WORKDIR /app

COPY . .

RUN make rmqpush && \
  ./build/build_linux_x64.sh && \
  cp /app/lib/linux_x64/*.so /outputs/lib && \
  cp /app/include/* /outputs/include 

RUN apt update && apt install -y swig

COPY --from=java /usr/lib/jvm/java-11-amazon-corretto/include /outputs/include

COPY Makefile.jvm dss_capi.i /outputs
RUN mkdir /outputs/java-lib
WORKDIR /outputs

RUN mkdir -p /app/build && \
    make -f Makefile.jvm all DSS_PATH=/outputs JAVA_PATH=/outputs OUTPUT_PATH=/outputs/java-lib && \
    rm -rf /app /outputs/Makefile.jvm /outputs/dss_capi*

FROM debian:trixie

COPY --from=builder /outputs /outputs
WORKDIR /outputs
