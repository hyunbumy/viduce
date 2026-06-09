# Local MoQ relay: the official moq-relay image with our dev config baked in.
FROM moqdev/moq-relay

# Our relay configuration (modeled on the upstream localhost.toml).
COPY relay.toml /app/relay.toml

# QUIC on UDP, plus the cert-fingerprint/HTTP/WebSocket endpoints on TCP — both 4443.
EXPOSE 4443/udp
EXPOSE 4443/tcp

# The base image's entrypoint is the relay binary, whose CLI is `moq-relay [FILE]`
# (the config is a positional argument, not a --config flag).
CMD ["/app/relay.toml"]
