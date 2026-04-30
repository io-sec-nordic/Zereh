# User Getting Started

This guide helps you run Zereh end-to-end for the first time.

## 1. Prerequisites

You need:

- Linux kernel 5.8+
- clang/llvm
- pkg-config
- libbpf development package
- libyaml development package
- root privileges for XDP attach/load

On Debian/Ubuntu, install dependencies with:

```bash
bash scripts/setup_dev.sh
```

Or validate manually:

```bash
make deps-check
```

## 2. Build

Compile all binaries and build metadata:

```bash
make -j"$(nproc)"
```

Expected outputs:

- build/bin/zerehctl
- build/bin/zereh_rx
- build/bin/mock_dns_server
- build/generated/zereh_build_info.h
- build/generated/zereh_license.txt

Check version info:

```bash
./build/bin/zerehctl version
```

## 3. Configure

Start from config.yaml and adjust:

- global.interface
- routes[].domain and target_type
- route-level filters

If you use xdp_prog routes, set either:

- routes[].prog_name (explicit symbol), or
- routes[].prog_array_index and provide zereh_app_<index> in the object, or
- rely on zereh_app_default fallback

## 4. Generate and Load

Generate + compile object from your config:

```bash
./build/bin/zerehctl generate -c config.yaml
```

Load on interface (example eth0):

```bash
sudo ./build/bin/zerehctl load -c config.yaml -i eth0
```

One-step pipeline:

```bash
sudo ./build/bin/zerehctl generate-load -c config.yaml -i eth0
```

## 5. Use Convenience Deploy Scripts

Development deploy:

```bash
bash scripts/deploy_dev.sh config.yaml eth0
```

Production deploy:

```bash
bash scripts/deploy_prod.sh config.yaml eth0
```

Canary deploy:

```bash
bash scripts/deploy_canary.sh config.yaml eth1 eth0 --promote
```

Both scripts now generate using the config path you pass.

Optional structured logs:

```bash
export ZEREH_LOG_MODE=structured
```

## 6. Validate Behavior

Quick checks:

```bash
./build/bin/zerehctl hash t.example.com fnv1a
./build/bin/zerehctl hash t.example.com siphash
```

Runtime checks:

- Confirm load output reports requested/effective XDP mode.
- Confirm packets are routed by target type.
- Confirm route filters behave as expected.

## 7. Safe Rollback

Detach using the CLI unload command:

```bash
sudo ./build/bin/zerehctl unload -c config.yaml -i eth0
```

Or use immediate kernel-level detachment:

```bash
sudo ip link set dev eth0 xdp off
```

To restore a previous release quickly:

```bash
bash scripts/rollback_release.sh previous eth0
```

Then fix config/build issues and re-run generate/load.
