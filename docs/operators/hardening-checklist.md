# Hardening Checklist

Use this checklist before production deployment.

## Host and Kernel

- [ ] Keep kernel updated with security fixes.
- [ ] Disable unnecessary network services on the host.
- [ ] Restrict shell access to trusted operators only.
- [ ] Enable host firewall rules for management ports.

## Runtime Privileges

- [ ] Limit sudo access for Zereh operators.
- [ ] Separate deploy user from interactive admin user.
- [ ] Restrict write access to config and script paths.
- [ ] Restrict execute permissions to approved binaries/scripts.

## Configuration Safety

- [ ] Review `default_action` choice explicitly before deploy.
- [ ] Validate `routes[]` coverage and fallback behavior.
- [ ] Validate filter bounds and rate limits against expected traffic.
- [ ] Validate `xdp_prog` symbol mapping (`prog_name` / fallback chain).

## Deployment Safety

- [ ] Use release directories with immutable artifact snapshots.
- [ ] Keep known-good config and generated object for rollback.
- [ ] Use `scripts/health_check.sh` before deploy.
- [ ] Test unload/rollback pathways in staging.

## Observability and Incident Readiness

- [ ] Capture deployment logs and retain them centrally.
- [ ] Record requested/effective xdp_mode after each deploy.
- [ ] Add alerting for traffic anomalies and drop spikes.
- [ ] Document emergency detach command for responders.

## Supply Chain and CI/CD

- [ ] Pin build dependencies where possible.
- [ ] Require CI pass before merge and release.
- [ ] Review release artifacts before publish.
- [ ] Keep CHANGELOG and upgrade notes current.

## Post-Deploy Verification

- [ ] Validate representative domains for each target type.
- [ ] Verify no unintended packet drops for allowed traffic.
- [ ] Verify blacklist and filter policies behave as intended.
- [ ] Verify rollback script can restore previous release.
