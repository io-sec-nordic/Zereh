#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/logging.sh"

cd "$REPO_ROOT"

CONFIG_PATH="${1:-config.yaml}"
IFACE_OVERRIDE="${2:-}"
WITH_LOAD="${3:-}"
COMPONENT="health-check"

if [[ "$WITH_LOAD" != "" && "$WITH_LOAD" != "--with-load" ]]; then
  zereh_log_error "$COMPONENT" "invalid third argument" "arg=$WITH_LOAD" "expected=--with-load"
  exit 1
fi

if [[ ! -f "$CONFIG_PATH" ]]; then
  zereh_log_error "$COMPONENT" "config file not found" "config=$CONFIG_PATH"
  exit 1
fi

zereh_log_info "$COMPONENT" "checking dependencies"
make deps-check

zereh_log_info "$COMPONENT" "building binaries"
make -j"$(nproc)"

zereh_log_info "$COMPONENT" "checking build metadata"
./build/bin/zerehctl version

zereh_log_info "$COMPONENT" "generating BPF object" "config=$CONFIG_PATH"
./build/bin/zerehctl generate -c "$CONFIG_PATH"

if [[ ! -f build/xdp/router_generated.c || ! -f build/xdp/router_generated.o ]]; then
  zereh_log_error "$COMPONENT" "generated artifacts missing" "dir=build/xdp"
  exit 1
fi

if [[ -n "$IFACE_OVERRIDE" ]]; then
  zereh_log_info "$COMPONENT" "checking interface exists" "iface=$IFACE_OVERRIDE"
  ip link show "$IFACE_OVERRIDE" >/dev/null

  zereh_log_info "$COMPONENT" "capturing current XDP state" "iface=$IFACE_OVERRIDE"
  ip -details link show dev "$IFACE_OVERRIDE" | grep -i xdp || true
fi

if [[ "$WITH_LOAD" == "--with-load" ]]; then
  if [[ -z "$IFACE_OVERRIDE" ]]; then
    zereh_log_error "$COMPONENT" "--with-load requires explicit interface override"
    exit 1
  fi

  ROOT_CMD=""
  if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
    ROOT_CMD="sudo"
  fi

  zereh_log_info "$COMPONENT" "running load probe" "iface=$IFACE_OVERRIDE"
  $ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH" -i "$IFACE_OVERRIDE"
fi

zereh_log_info "$COMPONENT" "health check passed" "config=$CONFIG_PATH" "iface=${IFACE_OVERRIDE:-none}"
