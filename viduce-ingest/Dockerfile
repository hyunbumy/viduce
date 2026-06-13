# Builds and runs the viduce-ingest binary. The crate lives in the root Cargo
# workspace, so this image must be built with the repo root as the build context
# (see docker-compose.yml: context is "." with dockerfile pointing here).
FROM docker.io/rust:1.92-bookworm AS build

# aws-lc-sys (pulled in by the MoQ/QUIC TLS stack) builds C code via cmake.
RUN apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*


WORKDIR /usr/src/viduce/viduce-ingest
COPY . .

RUN cargo build

# we must use entrypoint here since `depend` in docker-compose only takes effect at runtime, not build time
ENTRYPOINT ["cargo", "run"]

### Runtime image
# FROM docker.io/debian:bookworm-slim

# COPY --from=build /usr/src/viduce/target/debug/viduce-ingest /bin/viduce-ingest

# ENTRYPOINT ["/bin/viduce-ingest"]

