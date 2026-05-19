---
name: coding-agent-conventions
description: Project-level coding agent rules. Use for all tasks. Enforces tool usage conventions and coding standards.
---

# Coding Agent Conventions

## Tool Usage Rules

### CMake — NEVER run directly
- **Never** run `cmake`, `make`, `ninja`, or any build tool directly via `bash`.
- **Always** use the provided `cmake_configure` and `cmake_build` tools.
- These tools handle environment setup (e.g., VS Developer Environment), preset validation, and provide filtered build summaries.
- Don't delete the build directory. If you need to do that, ask the user for permission.