#!/bin/bash
# Dev environment (see docker-compose.dev.yml).
#
#   ./dev.sh                    # build + start in the background, then attach VS Code to `viduce-dev`
#   ./dev.sh exec dev bash      # shell into the container
#   ./dev.sh down               # stop and remove
#   ./dev.sh agent <cmd...>     # run <cmd...> in an isolated, ephemeral container (copies source, uses image build cache)
#   ./dev.sh agent-build        # rebuild the `viduce-dev` image (e.g. after dependency changes)
#
# Any other arguments are passed straight through to `compose`.
set -euo pipefail
cd "$(dirname "$0")"

if command -v podman >/dev/null; then engine=podman; else engine=docker; fi

if [ "${1:-}" = "agent-build" ]; then
    exec "$engine" build --target develop -t viduce-dev .
fi

if [ "${1:-}" = "agent" ]; then
    shift
    if [ $# -eq 0 ]; then set -- bash; fi
    tar --exclude='./viduce-engine/build' \
        --exclude='./target' \
        --exclude='*/.venv' \
        --exclude='./.git' \
        -cf - . | "$engine" run --rm -i \
        -w /usr/src/viduce \
        viduce-dev \
        bash -c 'tar -xf - && exec "$@"' _ "$@"
    exit $?
fi

if [ $# -eq 0 ]; then set -- up --build -d; fi
exec "$engine" compose -f docker-compose.dev.yml "$@"
