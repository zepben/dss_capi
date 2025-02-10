FROM ghcr.io/zepben/dss-capi-builder:latest-rust

RUN mkdir -p /app /outputs/lib /outputs/include
WORKDIR /app

COPY . .

RUN make rmqpush && \
  ./build/build_linux_x64.sh && \
  cp /app/lib/linux_x64/*.so /outputs/lib && \
  cp /app/include/* /outputs/include && \
  rm -rf /app

WORKDIR /outputs
