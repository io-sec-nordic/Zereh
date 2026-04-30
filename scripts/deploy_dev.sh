#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/logging.sh"

CONFIG_PATH="${1:-config.yaml}"
IFACE_OVERRIDE="${2:-}"
COMPONENT="deploy-dev"

ROOT_CMD=""
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  ROOT_CMD="sudo"
fi

zereh_log_info "$COMPONENT" "starting development deployment" "config=$CONFIG_PATH" "iface=${IFACE_OVERRIDE:-from_config}"

make deps-check
make all
./build/bin/zerehctl generate -c "$CONFIG_PATH"

if [[ -n "$IFACE_OVERRIDE" ]]; then
  zereh_log_info "$COMPONENT" "loading generated program on explicit interface" "iface=$IFACE_OVERRIDE"
  $ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH" -i "$IFACE_OVERRIDE"
else
  zereh_log_info "$COMPONENT" "loading generated program using config interface"
  $ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH"
fi

zereh_log_info "$COMPONENT" "development deployment complete"
