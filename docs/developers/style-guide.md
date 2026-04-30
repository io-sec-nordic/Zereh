# Style Guide

This guide defines coding and scripting style for Zereh.

## C and eBPF C Style

- Keep functions focused and single-purpose.
- Prefer explicit validation and early returns for error paths.
- Keep fast-path logic deterministic.
- Avoid dynamic route map lookup in routing dispatch path.
- Keep user-visible error messages actionable.

Formatting:

- Use clang-format for C/C headers.
- Run:

```bash
make format
make fmt-check
```

## Shell Script Style

- Start scripts with:

```bash
#!/usr/bin/env bash
set -euo pipefail
```

- Quote all variable expansions unless deliberate splitting is needed.
- Use helper functions for repeated behavior (for example logging).
- Keep scripts idempotent where possible.
- Print clear status and failure messages.

Formatting/linting:

- Use shfmt for formatting (via `make format` / `make fmt-check`).
- Use shellcheck for linting (via `make lint` when shellcheck is installed).

## Structured Logging Convention

Scripts and loader support optional structured logging mode.

Enable with:

```bash
export ZEREH_LOG_MODE=structured
```

Expected structured fields:

- ts
- level
- component
- msg

Use plain mode by default for local readability.

## Documentation Style

- Keep command examples copy-paste ready.
- Use role-based docs structure (`users`, `operators`, `developers`, `reference`, `release`).
- Update docs in the same change set when behavior or schema changes.

## Pre-PR Style Checklist

- [ ] `make format` executed (or rationale provided)
- [ ] `make fmt-check` passes
- [ ] `make lint` reviewed (where tools are available)
- [ ] Docs updated for user/operator visible changes
