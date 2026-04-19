# .ai/

This directory contains AI agent context and planning resources for the Viduce project.

## Development environment

Build, test, and run commands for native components (e.g., the C++ engine) must
be executed **inside the development container**, not on the host. Start it
from the repo root with `./start_devenv.sh` — this builds and attaches to the
`devenv` Podman container with the repo mounted at `/mnt/host/viduce`.

If the container is already running, exec into it instead of starting a new
one, e.g. `podman exec devenv bash -lc "cd /mnt/host/viduce/engine && <cmd>"`.
Do not attempt to run `cmake`, `ctest`, or other build tooling directly on the
host — toolchain and library versions are only guaranteed inside the container.

## Structure

```
.ai/
├── contexts/   # Project-wide context files loaded by AI agents
└── plans/      # Per-session work plans (not checked in)
```

### `contexts/`

Persistent context files intended to be loaded by AI agents (e.g., Claude Code) at the start of a session or task. Files here describe stable, project-wide information such as architecture, conventions, and domain knowledge. `project-detail.md` lives here.

Add a new file when a topic grows large enough to warrant its own document (e.g., `engine-internals.md`, `api-design.md`).

### `plans/`

Temporary work plans scoped to a single work session. Each file should capture the goal, approach, and task breakdown for that session. This directory is listed in `.gitignore` and is **not checked into source control** — plans are ephemeral and local to the developer's machine.
