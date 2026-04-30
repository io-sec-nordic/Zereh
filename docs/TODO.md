# Zereh TODO

This list tracks near-term and medium-term priorities for code, operations, and documentation.

## Core Engineering

- [ ] Add automated integration tests for generate/load flows with synthetic DNS packets.
- [ ] Add benchmark harness for throughput/latency comparisons across hash modes.
- [x] Add explicit CLI command to detach/unload XDP safely.
- [ ] Add CI matrix for multiple kernel versions and libbpf versions.
- [ ] Validate xdp_prog multi-symbol routing with dedicated test object and fixtures.

## Reliability and Operations

- [x] Add structured logging mode for loader and deploy scripts.
- [x] Add health check script for post-deploy validation.
- [x] Add canary deployment script variant with staged interface rollout.
- [x] Add rollback helper script that restores previous known-good release directory.
- [x] Add systemd unit examples for controlled startup/shutdown workflows.

## Security

- [ ] Add threat model document for route/filter bypass scenarios.
- [x] Add hardening checklist for production hosts.
- [ ] Add signed-release artifact verification workflow.

## Documentation

- [ ] Add packet-flow diagrams for each target type and fallback path.
- [x] Add cookbook of real-world config patterns (multi-tenant, geo routing, abuse filtering).
- [x] Add upgrade notes per version with schema migration guidance.
- [x] Add FAQ page for common deployment and kernel-driver questions.

## Developer Experience

- [x] Add format/lint targets and style guide for C and shell scripts.
- [ ] Add test fixtures for YAML parser edge cases and compatibility keys.
- [x] Add contribution guide with PR checklist and review expectations.
