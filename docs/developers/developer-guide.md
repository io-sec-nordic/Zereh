# Developer Guide

This guide covers local development workflow, coding touchpoints, and extension patterns for Zereh.

## 1. Local Setup

Install dependencies:

```bash
bash scripts/setup_dev.sh
```

Build:

```bash
make -j"$(nproc)"
```

Version sanity check:

```bash
./build/bin/zerehctl version
```

## 2. Project Layout

- control/
  - config model, YAML parser, code generation, compiler orchestration, loader
- xdp/
  - eBPF/XDP template and DNS parser
- apps/
  - helper binaries (AF_XDP receiver and mock DNS server)
- scripts/
  - setup, metadata generation, dev/prod deploy automation
- build/
  - compiled binaries, generated metadata, generated XDP source/object

## 3. Development Loop

1. edit code/config/template
2. build userspace binaries
3. generate XDP source and object
4. load/swap program on interface

Commands:

```bash
make -j"$(nproc)"
./build/bin/zerehctl generate -c config.yaml
sudo ./build/bin/zerehctl load -c config.yaml -i eth0
```

## 4. Where to Change What

### Add New Config Field

Update in order:

1. control/config.h (structs/enums/constants)
2. control/config.c (defaults)
3. control/yaml_parser.c (parse + validation + compatibility)
4. control/codegen.c (template substitutions if needed)
5. xdp/xdp_template.c (runtime behavior)
6. readme.md and docs/ (documentation updates)

### Add New Route Filter

1. extend route model in control/config.h
2. parse in control/yaml_parser.c
3. pass into generated route code in control/codegen.c
4. enforce in xdp/xdp_template.c filter function
5. verify with representative config and runtime tests

### Add New Tail Program Behavior

- Keep xdp_prog dispatch index-based in generated code.
- Program resolution is handled in loader when seeding prog_array.
- Maintain deterministic resolution order and explicit error messages.

## 5. Design Constraints

- Fast path avoids route map lookups by using generated switch/case.
- Parser must remain strict with bounds and type checks.
- Loader should fail explicitly when required resources/symbols are missing.
- Keep fallback behavior predictable (mode fallback, tail program fallback).

## 6. Quality Checklist Before Merge

- make -j"$(nproc)" passes
- make fmt-check reviewed
- make lint reviewed (if shellcheck is installed)
- zerehctl version still prints valid metadata
- config schema docs updated
- deploy scripts still consistent with CLI behavior
- no contradiction between readme.md and docs/

See `docs/developers/style-guide.md` for formatting and linting conventions.
