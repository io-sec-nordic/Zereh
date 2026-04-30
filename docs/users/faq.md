# FAQ

## Why does `make deps-check` fail on my host?

Required development dependencies are missing (typically clang, libbpf-dev, libyaml-dev, pkg-config). Install dependencies with:

```bash
bash scripts/setup_dev.sh
```

or install equivalent packages manually for your distro.

## Why can build succeed while generate/load fails?

Build can compile fallback stubs when headers/libs are missing. Runtime generate/load operations still require real libyaml/libbpf and clang toolchain.

## What is the fastest safe deployment path?

For iterative work:

```bash
bash scripts/deploy_dev.sh config.yaml <ifname>
```

For production-like rollout:

```bash
bash scripts/deploy_prod.sh config.yaml <ifname>
```

## Is there a staged canary rollout script?

Yes:

```bash
bash scripts/deploy_canary.sh config.yaml <canary_iface> [primary_iface] [--promote]
```

## How do I detach XDP safely?

Preferred path:

```bash
sudo ./build/bin/zerehctl unload -c config.yaml -i <ifname>
```

Emergency fallback:

```bash
sudo ip link set dev <ifname> xdp off
```

## Why did requested xdp_mode not stick?

Loader uses fallback order:

- offload -> native -> skb
- native -> skb

Check loader output for requested/effective mode.

## How does xdp_prog resolve target programs?

Per route resolution order:

1. `prog_name` (or `xdp_prog_name`)
2. `zereh_app_<prog_array_index>`
3. `zereh_app_default`

If none exist, load fails explicitly.

## How do I roll back quickly?

Use rollback helper:

```bash
bash scripts/rollback_release.sh [previous|latest|<release_dir>] [interface]
```

## Can logs be emitted in structured form?

Yes, set:

```bash
export ZEREH_LOG_MODE=structured
```

This enables structured logging mode for loader and deployment scripts.

## Where are role-based docs?

Start at `docs/README.md` and follow the user/operator/developer/release paths.
