# Versioning and Changelog Policy

This policy standardizes release numbering and changelog quality.

## Versioning Scheme

Zereh uses semantic versioning style:

- MAJOR.MINOR.PATCH
- Example: 1.4.2

Interpretation:

- MAJOR: incompatible behavior/config changes
- MINOR: backward-compatible feature additions
- PATCH: backward-compatible fixes and non-breaking improvements

Pre-release examples:

- 1.5.0-rc1
- 1.5.0-beta2

## What Triggers Each Bump

### MAJOR

- Breaking config schema change without compatibility path
- Behavioral changes that require operator action to preserve routing outcome
- Removal of previously documented command/behavior

### MINOR

- New route/filter capability
- New documented CLI/script behavior that is backward-compatible
- New operational tooling or documentation set

### PATCH

- Bug fixes
- Documentation corrections
- Script reliability fixes
- Internal refactors without external behavior change

## VERSION File Rules

- VERSION must reflect the intended release version.
- Update VERSION in the same PR as release prep.
- zerehctl version output should match VERSION + build revision metadata.

## Changelog Requirements

Maintain changelog entries per release with these sections:

- Added
- Changed
- Fixed
- Security
- Docs
- Internal

Entry quality rules:

- Describe user/operator visible impact first.
- Include migration notes when behavior changes.
- Mention fallback or compatibility implications explicitly.

## Changelog Entry Template

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- ...

### Changed
- ...

### Fixed
- ...

### Security
- ...

### Docs
- ...

### Internal
- ...
```

## Release Notes Alignment

Release notes (GitHub release text) should be derived from the same changelog content to avoid divergence.

## Hotfix Policy

For urgent production issues:

- Branch from latest stable tag.
- Apply minimal fix.
- Increment PATCH version.
- Document impact and rollback notes clearly.

## Documentation Update Policy

If config, routing behavior, deploy flow, or failure handling changes:

- update readme.md
- update relevant docs/ role guides
- include changelog entry

No release should ship with docs that contradict implementation.
