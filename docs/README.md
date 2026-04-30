# Zereh Documentation

This folder contains role-based documentation for using, operating, and extending Zereh.

## Who Should Read What

| Role | Start Here | Then Read |
| --- | --- | --- |
| User / Integrator | users/getting-started.md | users/configuration-guide.md, users/config-cookbook.md, users/faq.md |
| Operator / SRE | operators/production-runbook.md | operators/troubleshooting.md, operators/systemd-examples.md, operators/hardening-checklist.md |
| Developer / Contributor | contributors/contributing.md | developers/developer-guide.md, developers/style-guide.md, developers/architecture.md |
| Release Manager | release/release-checklist.md | release/versioning-changelog-policy.md, release/upgrade-notes.md |

## Documentation Map

- users/getting-started.md
  - Fast first run, dependency setup, build, generate, and load.
- users/configuration-guide.md
  - Complete practical configuration guidance with route and filter behavior.
- users/config-cookbook.md
  - Real-world config patterns for multi-tenant, geo split, and abuse resistance.
- users/faq.md
  - Common operational and configuration questions with concise answers.
- operators/production-runbook.md
  - Production rollout, verification, rollback, and safety checks.
- operators/troubleshooting.md
  - Common failure modes and direct fixes.
- operators/systemd-examples.md
  - Systemd service and timer examples for managed deployments.
- operators/hardening-checklist.md
  - Practical security and deployment hardening checklist.
- developers/developer-guide.md
  - Local development workflow and extension patterns.
- developers/style-guide.md
  - C/shell style conventions and format/lint workflow.
- developers/architecture.md
  - Internal control-plane/data-plane architecture and design constraints.
- contributors/contributing.md
  - PR workflow, coding standards, and review expectations.
- .github templates
  - pull request: `.github/pull_request_template.md`
  - issues: `.github/ISSUE_TEMPLATE/*`
- release/release-checklist.md
  - End-to-end pre-release, packaging, publish, and rollback checks.
- release/versioning-changelog-policy.md
  - Version bump rules and changelog policy for consistent releases.
- release/upgrade-notes.md
  - Template/process for documenting upgrade impact and migration notes.
- reference/command-reference.md
  - Make targets, zerehctl commands, and deploy scripts.
- TODO.md
  - Project roadmap and documentation backlog.

## Recommended Reading Paths

### New User Path

1. users/getting-started.md
2. users/configuration-guide.md
3. users/config-cookbook.md
4. users/faq.md

### Production Path

1. operators/production-runbook.md
2. operators/troubleshooting.md
3. operators/systemd-examples.md
4. operators/hardening-checklist.md

### Developer Path

1. contributors/contributing.md
2. developers/developer-guide.md
3. developers/style-guide.md
4. developers/architecture.md

### Release Path

1. release/release-checklist.md
2. release/versioning-changelog-policy.md
3. release/upgrade-notes.md

## Important Notes

- Build artifacts are generated in build/.
- `make deps-check` must pass before `generate`, `load`, `generate-load`, `deploy-dev`, or `deploy-prod`.
- XDP attach operations require root privileges.
