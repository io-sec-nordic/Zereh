# Release Checklist

Use this checklist for every planned release.

## 1. Scope Freeze and Readiness

- [ ] Confirm release scope (features, fixes, docs updates).
- [ ] Ensure open high-risk issues are triaged.
- [ ] Ensure TODO items not in scope are explicitly deferred.

## 2. Version and Metadata

- [ ] Update VERSION with target version.
- [ ] Confirm build metadata generation works:

```bash
make -j"$(nproc)"
./build/bin/zerehctl version
```

- [ ] Confirm revision fallback behavior is acceptable when git info is unavailable.

## 3. Quality Gates

- [ ] Local build succeeds.
- [ ] `make deps-check` passes in release environment.
- [ ] `make generate` succeeds with release config.
- [ ] Core command smoke checks pass:

```bash
./build/bin/zerehctl hash t.example.com fnv1a
./build/bin/zerehctl hash t.example.com siphash
./build/bin/zerehctl version
```

- [ ] CI workflow passes on release commit.

## 4. Documentation Gates

- [ ] readme.md reflects current behavior.
- [ ] docs/ guides updated for new/changed behavior.
- [ ] docs/release/versioning-changelog-policy.md followed.
- [ ] docs/release/upgrade-notes.md updated for this release.
- [ ] Changelog entry added for this release.

## 5. Packaging and Artifacts

- [ ] Build artifacts produced under build/.
- [ ] Release package includes expected files (build/, config.yaml, scripts, VERSION, docs if desired).
- [ ] License metadata file is present.

## 6. Tagging and Publishing

- [ ] Create annotated tag (vX.Y.Z).
- [ ] Push tag to trigger CD workflow.
- [ ] Verify uploaded release artifact.
- [ ] Verify release notes/changelog content.

## 7. Post-Release Verification

- [ ] Perform production-like deploy rehearsal with scripts/deploy_prod.sh.
- [ ] Verify xdp_mode fallback behavior is expected.
- [ ] Verify xdp_prog program resolution behavior for configured routes.

## 8. Rollback Preparedness

- [ ] Keep previous known-good artifact and config available.
- [ ] Confirm detach command documented:

```bash
sudo ip link set dev <ifname> xdp off
```

- [ ] Confirm known-good config reload procedure documented.

## 9. Release Sign-off

- [ ] Engineering sign-off
- [ ] Operations sign-off
- [ ] Documentation sign-off
