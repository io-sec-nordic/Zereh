# Configuration Cookbook

This cookbook provides practical configuration patterns you can adapt.

## Pattern 1: Multi-Tenant User-Space Routing

Use separate XSK queues per tenant with route-level rate limits.

```yaml
routes:
  - domain: "tenant-a.example.com"
    target_type: "user_space"
    xsk_map_index: 0
    filters:
      rate_limit:
        qps: 2000

  - domain: "tenant-b.example.com"
    target_type: "user_space"
    xsk_map_index: 1
    filters:
      rate_limit:
        qps: 1500
```

## Pattern 2: Geo or Edge Split to Kernel Targets

Forward specific domains to different upstream UDP ports.

```yaml
routes:
  - domain: "eu.example.com"
    target_type: "kernel_space"
    forward_port: 5302

  - domain: "us.example.com"
    target_type: "kernel_space"
    forward_port: 5303
```

## Pattern 3: Abuse-Resistant Route

Use PoW + bloom + type restrictions for high-risk domains.

```yaml
routes:
  - domain: "api.example.com"
    target_type: "xdp_prog"
    prog_array_index: 2
    prog_name: "zereh_app_secure"
    filters:
      pow:
        enabled: true
        difficulty: 22
        time_window: 60
      bloom_filter:
        enabled: true
        size: 4096
        hash_functions: 4
        time_window: 60
        allow_qnames:
          - "api.example.com"
          - "health.api.example.com"
      dns_types:
        allow: ["A", "AAAA"]
        deny: ["TXT", "ANY"]
```

## Pattern 4: Strict Query Shape

Constrain query depth and length for predictable traffic profiles.

```yaml
routes:
  - domain: "telemetry.example.com"
    target_type: "user_space"
    xsk_map_index: 3
    filters:
      qname_rules:
        min_labels: 4
        max_labels: 4
        min_length: 12
        max_length: 64
```

## Pattern 5: Global Baseline Guardrails

```yaml
global:
  interface: "eth0"
  xdp_mode: "native"
  default_action: "drop"

global_filters:
  ip_blacklist:
    - "10.0.0.0/8"
    - "192.0.2.0/24"
  max_packet_size: 512
  allowed_opcodes: [0]
  drop_truncated: true
```

## Tuning Notes

- Start with conservative rate limits and increase gradually.
- Keep bloom size large enough to reduce false positives.
- Prefer explicit `prog_name` for production xdp_prog routes.
- Use `default_action: drop` only with complete route coverage.
