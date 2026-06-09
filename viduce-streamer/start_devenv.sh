#!/bin/bash
set -euo pipefail

# Build and run the viduce-streamer development container.
# Mirrors the repo's root start_devenv.sh convention, but for the Node app.
cd "$(dirname "$0")"

# Build the dev image (installs npm deps via `npm ci`).
podman build -t viduce-streamer .

# Run the Vite dev server:
#   -p 5173:5173        forward the dev server to the host
#   -v "$(pwd)":/app    live-mount source for hot reload
#   -v /app/node_modules  anonymous volume keeps the image's deps under the mount
podman run --name viduce-streamer --rm -it \
  -p 5173:5173 \
  -v "$(pwd)":/app \
  -v /app/node_modules \
  viduce-streamer
