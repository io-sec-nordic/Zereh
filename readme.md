# Zereh (زره)

```text
_____               _     
 |__  /___ _ __ ___ | |__  
   / // _ \ '__/ _ \| '_ \ 
  / /|  __/ | |  __/| | | |
 /____\___|_|  \___||_| |_|

 eBPF/XDP Zero-Latency DNS Router & Edge Filter
```

## Overview
Zereh is a high-performance DNS router and edge filter built on eBPF/XDP. It runs at the earliest practical point in the Linux networking path to reduce per-packet overhead for DNS filtering and routing.

Instead of relying on route lookups in eBPF maps for every packet, Zereh uses a **code-generation and recompilation** model. Routing rules are compiled into generated C logic and loaded as an updated eBPF object with atomic link updates. This design minimizes fast-path routing overhead while supporting live configuration updates.

Performance characteristics depend on NIC, driver, kernel version, and route/filter complexity.

## Quick Start (Ubuntu/Debian)

```bash
bash scripts/setup_dev.sh
make deps-check
make
make generate
sudo ./build/bin/zerehctl generate-load -c config.yaml -i eth0
./build/bin/zerehctl version
```

To detach after testing:

```bash
sudo ./build/bin/zerehctl unload -c config.yaml -i eth0
```

## Documentation

- Docs hub: `docs/README.md`
- User guides: `docs/users/getting-started.md`, `docs/users/configuration-guide.md`, `docs/users/config-cookbook.md`, `docs/users/faq.md`
- Operator guides: `docs/operators/production-runbook.md`, `docs/operators/troubleshooting.md`, `docs/operators/systemd-examples.md`, `docs/operators/hardening-checklist.md`
- Developer guides: `docs/developers/developer-guide.md`, `docs/developers/style-guide.md`, `docs/developers/architecture.md`
- Contributor guide: `docs/contributors/contributing.md`
- Release docs: `docs/release/release-checklist.md`, `docs/release/versioning-changelog-policy.md`, `docs/release/upgrade-notes.md`
- GitHub templates: `.github/pull_request_template.md`, `.github/ISSUE_TEMPLATE/*`
- Command reference: `docs/reference/command-reference.md`
- Roadmap/TODO: `docs/TODO.md` (also mirrored by root `TODO.md`)

## Features
- **Static route dispatch in fast path:** No `bpf_map_lookup_elem` for route selection. Generated `switch/case` logic is used for dispatch.
- **Atomic program swaps:** Live updates are applied with `bpf_link_update` when link pinning is enabled.
- **3-way multi-target routing:**
  - **User space:** Direct delivery to `AF_XDP` consumers.
  - **Kernel space:** Destination-port rewrite + checksum update + `XDP_PASS`.
  - **XDP tail calls:** Route into app-specific XDP programs via `PROG_ARRAY`, with resolution order `prog_name` -> `zereh_app_<index>` -> `zereh_app_default`.
- **Strict C implementation:** Control plane and data plane are implemented in C.

## Architecture

1.  **Control Plane (`/control`)**: Parses the configuration file, generates `build/xdp/router_generated.c` mapping QNAME hashes to execution branches, invokes the Clang compiler, and performs an atomic swap of the eBPF program on the target network interface.
2.  **Data Plane (`/xdp`)**: The bare-metal eBPF program attached to the NIC. Parses Ethernet/IPv4/UDP headers, safely extracts the DNS QNAME, computes the hash, and executes the compiled jump table to route the packet.


### The Zereh Pipeline
```

                      +------------------------------------------+
                      |          Control Plane (User-Space)      |
                      |                                          |
   config.yaml --->   |  1. Parse YAML Config                    |
                      |  2. Generate C switch/case logic         |
                      |  3. Clang -> Compile to eBPF object      |
                      |  4. bpf_link_update (Atomic Swap)        |
                      +--------------------+---------------------+
                                           |
                                  (Loads eBPF bytecode)
                                           |
===========================================|===================================
                                           V
                      +------------------------------------------+
                      |            Data Plane (XDP/Kernel)       |
                      |                                          |
   [DNS UDP Packet] ->|  XDP NIC Driver (Zero-Copy)              |
                      |                                          |
                      |  +------------------------------------+  |
                      |  | 1. Parse ETH/IPv4/UDP headers      |  |
                      |  | 2. Extract DNS QNAME               |  |
                      |  | 3. Compute Hash (e.g., FNV-1a)     |  |
                      |  | 4. Execute Static Jump Table       |  |
                      |  +-----------------+------------------+  |
                      |                    |                     |
                      +--------------------+---------------------+
                                           |
                    +----------------------+----------------------+
                    |                      |                      |
            Target: t.example.com  Target: m.example.com  Target: q.example.com
            (user_space)           (kernel_space)         (xdp_prog)
                    |                      |                      |
                    V                      V                      V
            +--------------+       +--------------+       +--------------+
            | bpf_redirect |       | rewrite port |       | bpf_tail_call|
            | to XSKMAP    |       | + XDP_PASS   |       | to PROG_ARRAY|
            +------+-------+       +-------+------+       +-------+------+
                   |                       |                      |
===================|=======================|======================|============
                   |                       |                      |
                   V                       V                      V
            +--------------+       +---------------+      +---------------+
            |   AF_XDP     |       | Linux Network |      | App-Specific  |
            |  C Receiver  |       |     Stack     |      |  XDP Filter   |
            | (Zero-Copy)  |       |(UDP Port 5302)|      |  (e.g., PoW)  |
            +--------------+       +---------------+      +---------------+
```

## Prerequisites
- Linux Kernel 5.8+ (for `bpf_link` and modern XDP support)
- Clang / LLVM 10+
- `libbpf` and `libyaml`
- Root privileges for attaching XDP programs to network interfaces

## Licensing & Usage

**Zereh is distributed under a Dual-Licensing model.**

### 1. Open Source / Non-Commercial Use
For personal, academic, or non-commercial open-source projects, Zereh is licensed under the **GNU Affero General Public License v3.0 (AGPLv3)**.
Under this license:
*   You are free to use, modify, and distribute the software.
*   **Copyleft Condition:** If you modify the source code and run it as a service over a network, you MUST publish your modified source code. 

### 2. Commercial / Enterprise Use
Commercial use cases (including commercial hardware integration, proprietary backend deployment, or managed security offerings) are not covered by the AGPL option.

For commercial use, obtain a commercial license.


## Disclaimer
This software is provided "as is", without warranty of any kind, express or implied.

## Build

```bash
make
```

This builds:
- `build/bin/zerehctl` (YAML parser, code generator, compiler orchestration, atomic loader)
- `build/bin/zereh_rx` (AF_XDP receiver)
- `build/bin/mock_dns_server` (kernel-space redirect sink)
- `build/generated/zereh_build_info.h` (version/revision/timestamp)
- `build/generated/zereh_license.txt` (build-time license metadata)

If `libyaml` and `libbpf` development headers are missing, build still succeeds with fallback stubs, but `generate`/`load` will return explicit dependency errors at runtime.

Check required dependencies before generate/load:

```bash
make deps-check
```

Print build version and git revision:

```bash
./build/bin/zerehctl version
```

## Generate + Load Workflow

Generate and compile the XDP object from `config.yaml`:

```bash
make generate
```

Atomically load/swap on the configured interface:

```bash
make load
```

One-step generate + compile + atomic swap:

```bash
make generate-load
```

Convenience deploy targets:

```bash
make deploy-dev
make deploy-prod
```

## Control Plane Commands

```bash
# Generate + compile only
./build/bin/zerehctl generate -c config.yaml

# Load existing object using interface from config (global.interface)
sudo ./build/bin/zerehctl load -c config.yaml

# Load existing object with explicit interface override
sudo ./build/bin/zerehctl load -c config.yaml -i eth0

# Unload/detach using interface from config
sudo ./build/bin/zerehctl unload -c config.yaml

# Unload/detach with explicit interface override
sudo ./build/bin/zerehctl unload -c config.yaml -i eth0

# Full pipeline using interface from config
sudo ./build/bin/zerehctl generate-load -c config.yaml

# Full pipeline with explicit interface override
sudo ./build/bin/zerehctl generate-load -c config.yaml -i eth0

# Utility: print FNV-1a 64-bit hash for a domain
./build/bin/zerehctl hash t.example.com

# Utility: print SipHash-2-4 hash for a domain
./build/bin/zerehctl hash t.example.com siphash

# Utility: print build/version metadata
./build/bin/zerehctl version
```

## Configuration Schema

`config.yaml` supports both the new nested schema and a backward-compatible flat schema.

Top-level sections:
- `template_path`
- `generated_source`
- `generated_object`
- `xdp_section` (XDP program symbol name to load, default: `xdp_router`)
- `default_kernel_port` (default UDP destination port for `kernel_space` routes without `forward_port`, default: `5302`)
- `global`
  - `interface`
  - `xdp_mode`: `native`, `skb`, `offload`
  - `default_action`: `pass`, `drop`
- `global_filters`
  - `ip_blacklist` (CIDR list, loaded into `LPM_TRIE`)
  - `max_packet_size`
  - `allowed_opcodes`
  - `drop_truncated`
- `routes[]`
  - `domain`
  - `target_type`: `user_space`, `kernel_space`, `xdp_prog`
  - `xsk_map_index` (user_space)
  - `forward_port` (kernel_space)
  - `prog_array_index` (xdp_prog)
  - `prog_name` (xdp_prog, optional explicit program symbol in loaded object)
  - `filters.pow`
    - `enabled`
    - `difficulty`
    - `time_window`
  - `filters.qname_rules`
    - `prefix_match`
    - `suffix_match`
    - `min_labels` / `max_labels`
    - `min_length` / `max_length`
  - `filters.dns_types`
    - `allow`
    - `deny`
    - Supported values: `A`, `AAAA`, `TXT`, `ANY`
  - `filters.rate_limit`
    - `qps`
  - `filters.bloom_filter`
    - `enabled`
    - `size`
    - `hash_functions`
    - `time_window`
    - `allow_qnames` (optional list of QNAMEs to seed the bloom map)
    - aliases: `seed_qnames`, `seeds`
- `codegen_options`
  - `hash_algorithm`: `FNV-1a` or `SipHash`
  - `optimize_jump_tables`
  - `inline_checksums`

Control-plane compatibility keys still accepted:
- Flat globals: `interface`, `max_packet_size`, `allowed_opcodes`, `blacklist`, `drop_truncated`
- Flat route keys: `target`, `xsk_queue`, `kernel_port`, `prog_index`, `allow_types`, `pow_difficulty`, `xdp_prog_name`

Min/Max pairing rule:
- For `min_labels`/`max_labels` and `min_length`/`max_length`:
  - If only `max_*` is provided, `min_*` is auto-set to the same value.
  - If only `min_*` is provided, `max_*` is auto-set to the same value.
  - If neither is provided, defaults are used (`labels: 1..127`, `length: 1..255`).

### `xdp_prog` Multi-Program Example

```yaml
routes:
  - domain: "geo.example.com"
    target_type: "xdp_prog"
    prog_array_index: 1
    prog_name: "zereh_app_geo"

  - domain: "abuse.example.com"
    target_type: "xdp_prog"
    prog_array_index: 2
    prog_name: "zereh_app_abuse"

  - domain: "fallback.example.com"
    target_type: "xdp_prog"
    prog_array_index: 3
    # No prog_name here:
    # loader tries zereh_app_3, then zereh_app_default.
```

Notes:
- `prog_array_index` must be in range `0..63`.
- `prog_name` must match a real eBPF program symbol in the loaded object.
- If neither `prog_name`, nor `zereh_app_<prog_array_index>`, nor `zereh_app_default` exists, load fails explicitly.

## Implemented Data-Plane Behavior

- Hardened parser with bounds checks (`xdp/dns_parser.h`):
  - ETH/IPv4/UDP cursor parse
  - DNS header/question extraction
  - Lowercased canonical QNAME build
  - DNS TC flag extraction
- Global pre-route filters:
  - packet size cap
  - IPv4 blacklist (LPM_TRIE)
  - opcode mask filtering
  - truncated-query drop control
- Route dispatch:
  - generated static `switch/case` on precomputed route hash (FNV-1a or SipHash)
  - generated suffix fallback blocks for subdomain routing when exact hash does not match
  - user-space: `bpf_redirect_map(..., xsk_map, ...)`
  - kernel-space: UDP dport rewrite + checksum updates + `XDP_PASS`
  - xdp_prog: `bpf_tail_call(..., prog_array, index)`
    - control-plane seeds `prog_array[index]` using: `prog_name` -> `zereh_app_<index>` -> `zereh_app_default`
  - no hash-map lookup for route selection in fast path
- App-specific post-route filters:
  - prefix/suffix checks
  - label-depth constraints
  - QNAME length constraints
  - RR type allow/deny filtering (`A/AAAA/TXT/ANY`)
  - PoW difficulty with optional time window
  - Bloom filter with configurable bit-size/hash-functions/time-window
    - Bloom map seeding from control plane (`allow_qnames` or route-domain default seed)
  - per-route QPS rate limiting (`PERCPU_ARRAY` state)

## Runtime Notes

- `xdp_mode: native` uses link pinning and `bpf_link_update` for swap updates.
- `xdp_mode: skb/offload` uses mode-specific XDP attach flags with replacement semantics.
- Automatic attach fallback order:
  - requested `offload` -> `native` -> `skb`
  - requested `native` -> `skb`
- `xdp_prog` tail program resolution order:
  - explicit route `prog_name`
  - auto name `zereh_app_<prog_array_index>`
  - fallback `zereh_app_default`
- Full `generate` and `load` execution requires `libyaml` and `libbpf` development packages.

## Deployment Scripts

- Development deploy:

```bash
bash scripts/deploy_dev.sh config.yaml [interface]
```

- Production deploy:

```bash
bash scripts/deploy_prod.sh config.yaml [interface]
```

- Canary deploy:

```bash
bash scripts/deploy_canary.sh config.yaml <canary_iface> [primary_iface] [--promote]
```

- Health check:

```bash
bash scripts/health_check.sh [config.yaml] [interface] [--with-load]
```

`--with-load` requires an explicit `interface` argument.

- Rollback helper:

```bash
bash scripts/rollback_release.sh [previous|latest|<release_dir>] [interface]
```

- Structured logs (optional):

```bash
export ZEREH_LOG_MODE=structured
```

- Development dependency setup (Debian/Ubuntu):

```bash
bash scripts/setup_dev.sh
```

## CI/CD

- CI workflow: `.github/workflows/ci.yml`
  - installs dependencies
  - runs `make deps-check`
  - runs build + generate
  - uploads `build/` artifacts
- CD workflow: `.github/workflows/cd.yml`
  - runs on tags (`v*`) or manual dispatch
  - builds and packages release tarball
  - uploads artifact and publishes GitHub release on tags
