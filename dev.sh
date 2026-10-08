#!/bin/bash
# Dev environment (see docker-compose.dev.yml).
#
#   ./dev.sh                    # build + start in the background, then attach VS Code to `viduce-dev`
#   ./dev.sh exec dev bash      # shell into the container
#   ./dev.sh down               # stop and remove
#
# Any arguments are passed straight through to `compose`.
set -euo pipefail
cd "$(dirname "$0")"

if command -v podman >/dev/null; then engine=podman; else engine=docker; fi

if [ $# -eq 0 ]; then set -- up --build -d; fi
exec "$engine" compose -f docker-compose.dev.yml "$@"
