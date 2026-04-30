# Upgrade Notes Guide

Use this file as a living template for release upgrade notes.

## Purpose

Upgrade notes explain operationally relevant changes between versions, especially schema and behavior updates that can affect routing outcomes.

## Required Sections Per Release

For each release, add a section like:

```markdown
## Upgrade to X.Y.Z

### Breaking Changes
- ...

### Config Changes
- ...

### Runtime/Behavior Changes
- ...

### Action Required
1. ...
2. ...

### Validation Steps
- ...

### Rollback
- ...
```

## Baseline Entry

### Upgrade to 0.1.0

Breaking changes:

- None documented from prior public baseline.

Config and behavior highlights:

- Nested schema support with backward-compatible flat keys.
- xdp_prog route program resolution order:
  1. `prog_name` / `xdp_prog_name`
  2. `zereh_app_<prog_array_index>`
  3. `zereh_app_default`
- Deploy scripts generate from passed config path.
- `zerehctl unload` is available for explicit detach.
- Optional structured logging mode via `ZEREH_LOG_MODE=structured` for loader and deployment scripts.

Recommended operator actions:

1. Run `make deps-check` and install missing dependencies.
2. Run `scripts/health_check.sh` before rollout.
3. Validate xdp_prog symbol mapping in config.
4. Keep previous release directory for rollback (`scripts/rollback_release.sh`).
