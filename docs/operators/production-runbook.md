# Operator Production Runbook

This runbook is for deploying and operating Zereh in production-like environments.

## 1. Pre-Deployment Checklist

- Dependency gate passes:

```bash
make deps-check
```

- Correct interface name is known and stable.
- Config has been reviewed for:
  - route coverage
  - default_action choice
  - filter bounds/rate limits
  - xdp_prog program resolution correctness
- Rollback command is prepared:

```bash
sudo ip link set dev <ifname> xdp off
```

## 2. Build and Package

Clean build:

```bash
make clean
make -j"$(nproc)"
```

Generate object with intended config:

```bash
./build/bin/zerehctl generate -c config.yaml
```

Validate metadata:

```bash
./build/bin/zerehctl version
```

## 3. Production Deploy

Use the production helper script:

```bash
bash scripts/deploy_prod.sh config.yaml <ifname>
```

What it does:

1. dependency check
2. clean + rebuild
3. generate using provided config path
4. create timestamped release directory in build/release/
5. copy config and generated artifacts
6. load generated program on selected interface

Canary-first rollout (optional):

```bash
bash scripts/deploy_canary.sh config.yaml <canary_iface> <primary_iface> --promote
```

Enable structured logs for deploy/loader output (optional):

```bash
export ZEREH_LOG_MODE=structured
```

## 4. Post-Deploy Verification

Verify from deploy output:

- configured XDP program symbol loaded
- interface/ifindex correct
- requested and effective XDP mode

If you requested offload/native, verify fallback messages are understood.

Traffic validation:

- test representative domains per route
- confirm user_space/kernel_space/xdp_prog behavior
- validate rate-limit and filter behavior with controlled probes

## 5. Rollback Procedures

### Fast Detach Rollback

Preferred detach path:

```bash
sudo ./build/bin/zerehctl unload -c config.yaml -i <ifname>
```

Emergency fallback:

```bash
sudo ip link set dev <ifname> xdp off
```

### Config Rollback

1. restore last known-good config or choose a known release directory
2. regenerate object
3. load again

```bash
./build/bin/zerehctl generate -c <known-good.yaml>
sudo ./build/bin/zerehctl load -c <known-good.yaml> -i <ifname>
```

Or use release-directory rollback helper:

```bash
bash scripts/rollback_release.sh previous <ifname>
```

## 6. Release Artifacts

Expected release material under build/release/<timestamp>/:

- config.yaml
- zereh_license.txt (if generated)
- router_generated.c (if generated)
- router_generated.o (if generated)

Store release directories externally for audit/rollback traceability.

## 7. Security and Safety Notes

- Run load operations with least privilege controls around sudo/root.
- Keep config changes reviewed and versioned.
- Prefer staged rollout on non-critical interfaces before full traffic cutover.
