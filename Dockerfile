## Base image for development and build. Should only contain env setup
FROM rust:1.92-bookworm AS base 
RUN apt update

# Dependencies for cpp
RUN apt install -y build-essential cmake
# Depnedencies for ffmpeg
RUN apt install -y libavformat-dev libavcodec-dev libavutil-dev

## Image for building
FROM base AS build
WORKDIR /usr/src/viduce

# Build cpp engine
WORKDIR engine
RUN cmake -B build && cmake --build build
COPY build/lib/libengine_api.so /lib/
ENV LD_LIBRARY_PATH /lib

# TODO: Leverage Docker caching for Rust dependencies
WORKDIR /usr/src/viduce
COPY . .
RUN cargo build --release
COPY target/release/cmd /bin/viduce

CMD ["viduce", "engine"]

# TODO: Look into having a separate env for building and running to reduce Docker img size

