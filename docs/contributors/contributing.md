# Contributing Guide

This guide defines how to contribute code and documentation to Zereh with consistent quality.

## Contribution Scope

You can contribute:

- Control-plane changes (parser, codegen, compiler, loader)
- Data-plane changes (xdp template, parser safety, route/filter behavior)
- Tooling and scripts
- Documentation and examples
- Tests, verification scripts, and CI improvements

## Workflow Overview

1. Sync from latest default branch.
2. Create a short-lived feature/fix branch.
3. Make focused, reviewable commits.
4. Run local verification commands.
5. Open PR with a clear impact summary.
6. Address review feedback and keep PR up to date.

## Branch and Commit Practices

Recommended branch names:

- feat/<short-topic>
- fix/<short-topic>
- docs/<short-topic>
- chore/<short-topic>

Commit guidance:

- Keep each commit logically scoped.
- Separate refactors from behavior changes when possible.
- Use descriptive messages that explain intent and impact.

Example commit subjects:

- feat(loader): resolve xdp_prog programs by explicit symbol before fallback
- fix(deploy): generate from provided config path in prod script
- docs(operators): add runbook rollback verification steps

## Pull Request Requirements

Every PR should include:

- Problem statement
- Solution summary
- Risk assessment (routing correctness, load behavior, compatibility)
- Validation evidence (commands and observed outputs)
- Documentation updates when behavior/config changes

PR checklist:

- [ ] Build passes locally (`make -j"$(nproc)"`)
- [ ] Dependency gate result documented (`make deps-check`)
- [ ] Formatting check reviewed (`make fmt-check`)
- [ ] Affected commands tested (`zerehctl` and/or scripts)
- [ ] Documentation updated (`readme.md` and/or `docs/`)
- [ ] No unrelated file churn

## GitHub Templates

Use the built-in templates when opening issues and pull requests:

- Pull request template:
	- `.github/pull_request_template.md`
- Issue templates:
	- `.github/ISSUE_TEMPLATE/bug_report.md`
	- `.github/ISSUE_TEMPLATE/feature_request.md`
	- `.github/ISSUE_TEMPLATE/docs_improvement.md`

Template usage rules:

- Fill every required section with concrete details.
- Provide reproducible commands and outputs for bug reports.
- Include impact and rollback notes for risky PRs.
- Link docs updates for config or behavior changes.

## Coding Standards

### C and eBPF C

- Keep functions small and explicit.
- Favor deterministic logic and explicit error paths.
- Validate all parsed input and bounds.
- Preserve fast-path constraints (avoid dynamic route map lookup in dispatch path).
- Keep route behavior deterministic and observable.

### Shell Scripts

- Use `set -euo pipefail`.
- Quote variables.
- Emit explicit, actionable error messages.
- Keep script behavior aligned with command-line options and docs.

### Markdown Docs

- Keep examples copy-paste ready.
- Prefer role-based organization (users/operators/developers/reference).
- Update docs in the same PR as behavior changes.

Style reference:

- `docs/developers/style-guide.md`

## Testing and Validation Expectations

Minimum validation for most changes:

```bash
make -j"$(nproc)"
make fmt-check
./build/bin/zerehctl version
```

For parser/codegen/loader changes, additionally run:

```bash
./build/bin/zerehctl generate -c config.yaml
# requires root and dependencies for actual load
sudo ./build/bin/zerehctl load -c config.yaml -i <ifname>
```

For deployment script changes, run script dry and real paths in a safe environment.

## Review Guidelines

Reviewers focus on:

- Functional correctness
- Routing/filter behavior regressions
- Failure mode clarity and operator safety
- Documentation accuracy and completeness

Authors should respond with:

- What changed
- Why this approach was chosen
- How regressions were considered

## Security and Reliability Notes

- Treat all config input as untrusted until validated.
- Avoid silent fallbacks that hide misconfiguration.
- Keep rollback pathways explicit and documented.
- Preserve predictable attach behavior and clear logs.

## Documentation Rule

Any change to config schema, route behavior, deployment semantics, or CLI usage must update relevant docs in the same PR.
