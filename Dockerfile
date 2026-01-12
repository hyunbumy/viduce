## Base image for development and build. Should only contain env setup
FROM rust:1.92-bookworm AS base 
RUN apt update

# Dependencies for cpp
RUN apt install -y build-essential cmake
# Depnedencies for ffmpeg
RUN apt install -y libavformat-dev libavcodec-dev

# Static linking for rust
RUN apt install -y musl-tools musl-dev
RUN rustup target add x86_64-unknown-linux-musl

## Image for building
FROM base AS build
WORKDIR /usr/src/viduce
# TODO: Leverage Docker caching for Rust dependencies
COPY . .
RUN cargo build --release --target x86_64-unknown-linux-musl

## Image for deployment
FROM scratch
COPY --from=build /usr/src/viduce/target/x86_64-unknown-linux-musl/release/cmd /bin/viduce
CMD ["viduce", "realesrgan"]
