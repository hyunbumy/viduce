# Allow build type specification
ARG BUILD_TYPE="debug"

### Base image for development and build. Should only contain env setup
FROM rust:1.92-bookworm AS base 
RUN apt update

# Dependencies for cpp
RUN apt install -y build-essential cmake
# Depnedencies for ffmpeg
RUN apt install -y libavformat-dev libavcodec-dev libavutil-dev

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
RUN cmake -B build -DCMAKE_BUILD_TYPE=${ENGINE_BUILD}
RUN cmake --build build -j 18
RUN cp build/lib/libengine_api.so /lib/
ENV LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/lib"

# Build Rust
# TODO: Leverage Docker caching for Rust dependencies
WORKDIR /usr/src/viduce
RUN cargo build ${SERVICE_BUILD}
RUN cp target/${SERVICE_PATH}/cmd /bin/viduce

### Execution stage
# TODO: Look into having a separate env for building and running to reduce Docker img size
FROM build AS run
CMD ["viduce", "engine"]

