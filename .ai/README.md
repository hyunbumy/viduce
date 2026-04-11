# .ai/

This directory contains AI agent context and planning resources for the Viduce project.

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
