# Command Reference

This page centralizes frequently used make targets, CLI commands, and script entrypoints.

## Make Targets

Build everything:

```bash
make -j"$(nproc)"
```

Dependency gate:

```bash
make deps-check
```

Generate object from config.yaml:

```bash
make generate
```

Load object from config.yaml:

```bash
make load
```

Generate and load in one step:

```bash
make generate-load
```

Deploy helpers:

```bash
make deploy-dev
make deploy-prod
make deploy-canary CANARY_IFACE=eth1 PRIMARY_IFACE=eth0 PROMOTE=1
```

Health and rollback helpers:

```bash
make health-check
make rollback-release
```

Formatting and linting:

```bash
make format
make fmt-check
make lint
```

Clean artifacts:

```bash
make clean
```

## zerehctl Commands

Generate:

```bash
./build/bin/zerehctl generate -c config.yaml
```

Load:

```bash
sudo ./build/bin/zerehctl load -c config.yaml
sudo ./build/bin/zerehctl load -c config.yaml -i eth0
```

Unload/detach:

```bash
sudo ./build/bin/zerehctl unload -c config.yaml
sudo ./build/bin/zerehctl unload -c config.yaml -i eth0
```

Generate and load:

```bash
sudo ./build/bin/zerehctl generate-load -c config.yaml
sudo ./build/bin/zerehctl generate-load -c config.yaml -i eth0
```

Notes:

- `-i` is an optional interface override. Without `-i`, interface comes from config (`global.interface`).

Hash utility:

```bash
./build/bin/zerehctl hash t.example.com fnv1a
./build/bin/zerehctl hash t.example.com siphash
```

Version metadata:

```bash
./build/bin/zerehctl version
```

## Scripts

Dependency bootstrap (Debian/Ubuntu):

```bash
bash scripts/setup_dev.sh
```

Development deployment:

```bash
bash scripts/deploy_dev.sh config.yaml [interface]
```

Production deployment:

```bash
bash scripts/deploy_prod.sh config.yaml [interface]
```

Canary deployment:

```bash
bash scripts/deploy_canary.sh config.yaml <canary_iface> [primary_iface] [--promote]
```

Build metadata generation:

```bash
bash scripts/gen_build_metadata.sh [out_dir] [version] [revision]
```

Health check:

```bash
bash scripts/health_check.sh [config.yaml] [interface] [--with-load]
```

Notes:

- `--with-load` requires an explicit `interface` argument.

Rollback to previous/latest/specific release directory:

```bash
bash scripts/rollback_release.sh [previous|latest|<release_dir>] [interface]
```

## Logging Mode

Enable structured logs for scripts and loader:

```bash
export ZEREH_LOG_MODE=structured
```

## Emergency Command

Detach XDP:

```bash
sudo ip link set dev <ifname> xdp off
```
