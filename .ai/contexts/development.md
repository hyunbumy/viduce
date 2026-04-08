# AI Development Guidelines

This document outlines the expected behavior and workflow for AI agents working on the Viduce project.

## Workflow Principles

### 1. Logical Scoping
- All code changes should be in a **complete logical scope**.
- **Partial implementations or TODOs are acceptable** if they make it easier to understand the logical scope or represent a necessary step in a larger refactoring.

### 2. Verification Before Approval
- AI agents should always **ask for approval only once** the suggested changes have been **verified**.
- Verification must be performed within the **development docker environment** (use `./start_devenv.sh`).
- Once verified, the suggestion presented to the user should include a **full diff** of all proposed changes.

## Technical Checklist

Before suggesting a change and asking for approval, ensure the following have been verified inside the Docker container:

- [ ] **Build Integrity**: `cargo build` and `cmake --build engine/build` pass without errors.
- [ ] **Tests**: Relevant Rust and C++ tests pass (`cargo test`, `ctest`).
- [ ] **Linting**: Code adheres to project formatting and linting standards.
    - **C++**: Follow [Google's C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
- [ ] **Environment**: All verification steps were executed within the project's development Docker environment.
