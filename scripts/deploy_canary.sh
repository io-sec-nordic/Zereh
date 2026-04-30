#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/logging.sh"

cd "$REPO_ROOT"

CONFIG_PATH="${1:-config.yaml}"
CANARY_IFACE="${2:-}"
PRIMARY_IFACE="${3:-}"
PROMOTE_FLAG="${4:-}"
COMPONENT="deploy-canary"

if [[ -z "$CANARY_IFACE" ]]; then
  echo "usage: $0 <config.yaml> <canary_iface> [primary_iface] [--promote]" >&2
  exit 1
fi

if [[ "$PROMOTE_FLAG" != "" && "$PROMOTE_FLAG" != "--promote" ]]; then
  echo "invalid 4th argument: $PROMOTE_FLAG (expected --promote)" >&2
  exit 1
fi

if [[ ! -f "$CONFIG_PATH" ]]; then
  zereh_log_error "$COMPONENT" "config file not found" "config=$CONFIG_PATH"
  exit 1
fi

ROOT_CMD=""
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  ROOT_CMD="sudo"
fi

zereh_log_info "$COMPONENT" "starting canary deployment" "config=$CONFIG_PATH" "canary_iface=$CANARY_IFACE" "primary_iface=${PRIMARY_IFACE:-none}" "promote=$([[ "$PROMOTE_FLAG" == "--promote" ]] && echo yes || echo no)"

make deps-check
make clean
make -j"$(nproc)"
./build/bin/zerehctl generate -c "$CONFIG_PATH"

zereh_log_info "$COMPONENT" "loading canary interface" "iface=$CANARY_IFACE"
$ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH" -i "$CANARY_IFACE"

zereh_log_info "$COMPONENT" "running health check on canary" "iface=$CANARY_IFACE"
bash "$SCRIPT_DIR/health_check.sh" "$CONFIG_PATH" "$CANARY_IFACE"

if [[ -n "$PRIMARY_IFACE" ]]; then
  if [[ "$PROMOTE_FLAG" == "--promote" ]]; then
    zereh_log_info "$COMPONENT" "promoting canary to primary interface" "iface=$PRIMARY_IFACE"
    $ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH" -i "$PRIMARY_IFACE"
    zereh_log_info "$COMPONENT" "primary promotion complete" "iface=$PRIMARY_IFACE"
  else
    zereh_log_warn "$COMPONENT" "canary complete; primary not promoted" "iface=$PRIMARY_IFACE" "hint=rerun_with_--promote"
  fi
fi

zereh_log_info "$COMPONENT" "canary deployment flow completed"
