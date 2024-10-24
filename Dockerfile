FROM ghcr.io/zepben/dss-capi-builder:latest

RUN mkdir -p /app /outputs
WORKDIR /app

COPY . .

RUN make rmqpush && ./build/build_linux_x64.sh && cp /app/lib/linux_x64/*.so /outputs
