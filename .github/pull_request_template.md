## Summary

Describe what changed and why.

## Change Type

- [ ] Feature
- [ ] Bug fix
- [ ] Refactor
- [ ] Documentation
- [ ] Build/CI/CD
- [ ] Other

## Problem and Context

What problem does this PR solve?

## Behavior Impact

- Affected components:
  - [ ] control plane
  - [ ] data plane
  - [ ] deploy scripts
  - [ ] docs
  - [ ] CI/CD
- Routing/filter behavior changed?
  - [ ] Yes
  - [ ] No

If yes, explain expected behavior changes.

## Config and Compatibility

- Config schema changed?
  - [ ] Yes
  - [ ] No
- Backward compatibility preserved?
  - [ ] Yes
  - [ ] No
  - [ ] Not applicable

If compatibility is impacted, provide migration notes.

## Validation

Commands executed and results:

```bash
# paste commands and key output here
```

Suggested baseline:

- `make -j"$(nproc)"`
- `./build/bin/zerehctl version`
- `./build/bin/zerehctl generate -c config.yaml` (when applicable)
- `sudo ./build/bin/zerehctl load -c config.yaml -i <ifname>` (when applicable)

## Risk and Rollback

- Risk level:
  - [ ] Low
  - [ ] Medium
  - [ ] High
- Rollback plan documented?
  - [ ] Yes
  - [ ] No

Describe rollback procedure for this change.

## Documentation

- [ ] `readme.md` updated if behavior/config changed
- [ ] `docs/` updated if needed
- [ ] `CHANGELOG.md` updated (if release-impacting)

## PR Checklist

- [ ] Changes are focused and scoped
- [ ] No unrelated files changed
- [ ] Error handling is explicit
- [ ] Fast-path constraints preserved (no dynamic route map lookup in dispatch path)
- [ ] Reviewer can reproduce validation steps
