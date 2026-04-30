# Architecture Deep Dive

Zereh consists of a control plane that generates and loads eBPF programs, and a data plane that processes DNS traffic at XDP layer.

## 1. End-to-End Pipeline

1. Parse YAML config into strongly-typed config model.
2. Generate route-specific C source from xdp template.
3. Compile generated C with clang target bpf.
4. Load object with libbpf.
5. Seed maps (blacklist, bloom, prog_array).
6. Attach/swap program on target interface.

## 2. Control Plane Components

### YAML Parsing

- Reads nested and compatibility schema keys.
- Applies strict validation and defaults.
- Performs min/max auto-pairing for route constraints.

### Code Generation

- Injects route switch/case blocks.
- Injects suffix fallback blocks.
- Injects compile-time constants for packet size, opcode mask, hash mode, and default action.

### Compilation

- Invokes clang with bpf target.
- Supports jump table optimization toggle.

### Loader

- Loads object and resolves configured XDP program symbol (xdp_section).
- Seeds IP blacklist map.
- Seeds bloom map from configured seed qnames.
- Seeds prog_array for xdp_prog routes using deterministic program resolution.
- Attaches program with mode-specific behavior and fallback.

## 3. Data Plane Components

### Parsing and Safety

- ETH/IPv4/UDP parse with bounds checks.
- DNS query parse with canonical lowercased qname extraction.
- Extracts opcode, qtype, and TC flag.

### Pre-Route Global Filters

- packet size limit
- source IP blacklist
- opcode allow mask
- truncated drop policy

### Route Dispatch and Post-Route Filters

- dispatch by precomputed hash switch/case
- fallback suffix matching for subdomains
- route action:
  - user_space -> redirect to xsk_map
  - kernel_space -> rewrite UDP destination and pass
  - xdp_prog -> tail call via prog_array index
- post-route checks:
  - prefix/suffix
  - label depth and qname length
  - RR type mask
  - PoW
  - bloom membership
  - per-route rate limiting

## 4. Hashing Model

- FNV-1a path uses parsed qname hash.
- SipHash path computes hash in XDP program with static keys.
- Hash algorithm selection is configured by codegen option.

## 5. Program Resolution for xdp_prog Routes

For each xdp_prog route:

1. explicit prog_name/xdp_prog_name
2. zereh_app_<prog_array_index>
3. zereh_app_default

If none resolve, loader fails to prevent silent misrouting.

## 6. Attach and Swap Semantics

- native mode: pinned link and bpf_link_update for atomic updates
- skb/offload mode: mode-specific XDP attach replacement
- fallback order:
  - offload -> native -> skb
  - native -> skb

## 7. Why Static Codegen Matters

Static route codegen allows route selection via compiled branching instead of runtime map lookup, preserving predictable fast-path performance at high packet rates.
