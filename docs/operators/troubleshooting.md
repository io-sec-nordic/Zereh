# Operator Troubleshooting

This guide lists common failures and fast remediation steps.

## Dependency Gate Fails

Symptom:

- `make deps-check` reports missing clang/libbpf/libyaml.

Fix:

```bash
bash scripts/setup_dev.sh
# or install missing packages manually for your distro
```

Re-test:

```bash
make deps-check
```

## Generate Fails

Symptom:

- `zerehctl generate` returns parse or compile error.

Likely causes:

- invalid YAML shape/value
- unsupported enum string
- invalid min/max bounds
- missing clang

Fix:

1. validate schema keys and value ranges
2. check config path passed to command
3. rerun with corrected config

## Load Fails: Interface Not Found

Symptom:

- `if_nametoindex failed`

Fix:

- verify interface name exists:

```bash
ip link show
```

- pass explicit interface:

```bash
sudo ./build/bin/zerehctl load -c config.yaml -i <ifname>
```

## Load Fails: xdp_prog Program Not Found

Symptom:

- route requests prog_name but loader cannot find symbol
- no prog_name and no zereh_app_<index> and no zereh_app_default

Fix:

- set correct routes[].prog_name
- or include matching zereh_app_<prog_array_index>
- or include zereh_app_default fallback program

## Attach Mode Fallback Happens Unexpectedly

Symptom:

- offload/native request falls back to another mode

Fix:

- check NIC/driver support
- verify kernel and driver capabilities
- accept fallback or explicitly request supported mode

## Map Update Errors

Symptom:

- bpf_map_update_elem errors during blacklist/prog_array/bloom seeding

Fix:

- check route indices and bloom settings
- ensure prog_array_index in 0..63
- reduce malformed or excessive seed input

## Emergency Recovery

Detach with unload command:

```bash
sudo ./build/bin/zerehctl unload -c config.yaml -i <ifname>
```

Immediate fallback detach:

```bash
sudo ip link set dev <ifname> xdp off
```

Then return to known-good config and redeploy.
