# User Configuration Guide

This guide explains how config.yaml is interpreted by the control plane and used by the generated XDP program.

## Top-Level Fields

- template_path
  - Path to xdp template source.
- generated_source
  - Output C file generated from template and routes.
- generated_object
  - Compiled eBPF object.
- xdp_section
  - XDP program symbol name to load from generated object (default: xdp_router).
- default_kernel_port
  - Default UDP destination port used for kernel_space routes when forward_port is omitted (default: 5302).
- global
  - interface, xdp_mode, default_action.
- global_filters
  - ip_blacklist, max_packet_size, allowed_opcodes, drop_truncated.
- routes
  - Routing table entries with target-specific behavior and filters.
- codegen_options
  - hash_algorithm, optimize_jump_tables, inline_checksums.

## Global Section

- interface
  - NIC name used for load if no CLI `-i` override is provided.
- xdp_mode
  - native, skb, offload.
  - Attach fallback is automatic in loader:
    - offload -> native -> skb
    - native -> skb
- default_action
  - pass or drop when no route or fallback matches.

## Global Filters

- ip_blacklist
  - CIDR list loaded into LPM trie map.
- max_packet_size
  - Drop packets above this size.
- allowed_opcodes
  - Bitmask built from allowed DNS opcode values.
- drop_truncated
  - Drop DNS queries with TC bit set.

## Route Targets

Each route has:

- domain
- target_type: user_space | kernel_space | xdp_prog

Target-specific fields:

- user_space
  - xsk_map_index
- kernel_space
  - forward_port (optional; falls back to default_kernel_port)
- xdp_prog
  - prog_array_index
  - prog_name (optional explicit symbol)

### xdp_prog Program Resolution Order

For each xdp_prog route, loader resolves program binding in this order:

1. prog_name (or compatibility alias xdp_prog_name)
2. zereh_app_<prog_array_index>
3. zereh_app_default

If none exist, load fails.

## Route Filters

routes[].filters supports:

- pow
  - enabled, difficulty, time_window
- qname_rules
  - prefix_match, suffix_match, min_labels, max_labels, min_length, max_length
- dns_types
  - allow, deny
  - Supported: A, AAAA, TXT, ANY
- rate_limit
  - qps (per-route)
- bloom_filter
  - enabled, size, hash_functions, time_window, allow_qnames
  - aliases for seeds: seed_qnames, seeds

## Min/Max Auto-Pairing

The parser auto-pairs missing bounds:

- min_labels only -> max_labels = min_labels
- max_labels only -> min_labels = max_labels
- min_length only -> max_length = min_length
- max_length only -> min_length = max_length

Defaults when both omitted:

- labels: 1..127
- qname length: 1..255

## Backward-Compatible Keys

Accepted older keys:

- Global: interface, max_packet_size, allowed_opcodes, blacklist, drop_truncated
- Route: target, xsk_queue, kernel_port, prog_index, allow_types, pow_difficulty, xdp_prog_name

## Practical Example Snippet

```yaml
routes:
  - domain: "app.example.com"
    target_type: "user_space"
    xsk_map_index: 0
    filters:
      rate_limit:
        qps: 500
      dns_types:
        allow: ["A", "AAAA"]

  - domain: "legacy.example.com"
    target_type: "kernel_space"
    forward_port: 5302

  - domain: "secure.example.com"
    target_type: "xdp_prog"
    prog_array_index: 3
    prog_name: "zereh_app_secure"
```
