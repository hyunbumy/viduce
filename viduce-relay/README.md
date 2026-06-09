# viduce-relay

A local [MoQ](https://github.com/moq-dev/moq) relay for development, packaged as a
container. It bakes a local-dev config onto the official `moqdev/moq-relay` image to give
local development a reproducible relay to publish to and subscribe from.

## What it does

- Runs `moq-relay` listening for WebTransport (QUIC) on **UDP 4443**.
- Serves the self-signed cert fingerprint and HTTP/WebSocket fallbacks on **TCP 4443**.
- Anonymous auth — any broadcast path, no token (local dev only).

## TLS / certificates

`relay.toml` sets `tls.generate = ["localhost"]`: the relay self-signs at startup and
exposes its SHA-256 fingerprint at `http://localhost:4443/certificate.sha256`. A MoQ
client fetches that fingerprint and pins it via WebTransport `serverCertificateHashes`,
so **no CA import or browser trust changes are needed**.

The generated cert is short-lived (Chrome caps local certs at ~14 days), but it is
regenerated every time the relay starts, so this is not a concern for development.

## Run it

```bash
podman build -t viduce-relay .
podman run --rm -p 4443:4443/udp -p 4443:4443/tcp viduce-relay
```

Docker works too — substitute `docker` for `podman`.

Verify it's up:

```bash
curl http://127.0.0.1:4443/certificate.sha256   # should print a hash
```

## Connecting a client

Point a MoQ client's relay URL at `https://localhost:4443`. The client discovers the cert
fingerprint automatically via the endpoint above.
