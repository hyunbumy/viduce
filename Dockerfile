# Allow build type specification
ARG BUILD_TYPE="debug"

### Base image for development and build. Should only contain env setup
FROM docker.io/rust:1.92-bookworm AS base 
RUN apt update && apt install -y \
    # Sys deps
    git-lfs \
    # Dependencies for cpp
    build-essential clang cmake gdb \
    # Depnedencies for ffmpeg
    libavformat-dev libavcodec-dev libavutil-dev \
    # Dependencies for opencv
    libopencv-dev \
    # Node
    nodejs npm

# Python dependencies
# Setup uv
RUN curl -LsSf https://astral.sh/uv/install.sh | sh

### Development image
FROM base AS develop

# Install dev rust deps
RUN rustup component add rust-docs rustfmt clippy

### Temporary stages for build type
FROM base AS build-debug
ENV ENGINE_BUILD="Debug"
ENV SERVICE_BUILD=""
ENV SERVICE_PATH="debug"

FROM base AS build-release
ENV ENGINE_BUILD="Release"
ENV SERVICE_BUILD="--release"
ENV SERVICE_PATH="release"

### Build stage
FROM build-$BUILD_TYPE AS build
WORKDIR /usr/src/viduce
COPY . .

# Build cpp engine
WORKDIR engine
RUN cmake -B build -DCMAKE_BUILD_TYPE=${ENGINE_BUILD} -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
RUN cmake --build build -j8

WORKDIR build
RUN ctest -VV

RUN cp lib/libengine_api.so /lib/

# Build Rust
# TODO: Leverage Docker caching for Rust dependencies
WORKDIR /usr/src/viduce
RUN cargo test

RUN cargo build ${SERVICE_BUILD}
RUN cp target/${SERVICE_PATH}/cmd /bin/viduce

### Execution stage
## Image for deployment
FROM docker.io/debian:bookworm-slim

RUN apt update
RUN apt install -y ffmpeg

COPY --from=build /bin/viduce /bin/viduce
COPY --from=build /lib/libengine_api.so /lib/

ENTRYPOINT ["/bin/viduce"]
