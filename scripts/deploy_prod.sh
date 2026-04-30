#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/logging.sh"

CONFIG_PATH="${1:-config.yaml}"
IFACE_OVERRIDE="${2:-}"
RELEASE_DIR="build/release/$(date -u +%Y%m%dT%H%M%SZ)"
COMPONENT="deploy-prod"

ROOT_CMD=""
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  ROOT_CMD="sudo"
fi

zereh_log_info "$COMPONENT" "starting production deployment" "config=$CONFIG_PATH" "iface=${IFACE_OVERRIDE:-from_config}" "release_dir=$RELEASE_DIR"

make deps-check
make clean
make -j"$(nproc)"
./build/bin/zerehctl generate -c "$CONFIG_PATH"

mkdir -p "$RELEASE_DIR"
cp -f "$CONFIG_PATH" "$RELEASE_DIR/config.yaml"
if [[ -f build/generated/zereh_license.txt ]]; then
  cp -f build/generated/zereh_license.txt "$RELEASE_DIR/"
fi
if [[ -f build/xdp/router_generated.o ]]; then
  cp -f build/xdp/router_generated.o "$RELEASE_DIR/"
fi
if [[ -f build/xdp/router_generated.c ]]; then
  cp -f build/xdp/router_generated.c "$RELEASE_DIR/"
fi

if [[ -n "$IFACE_OVERRIDE" ]]; then
  zereh_log_info "$COMPONENT" "loading generated program on explicit interface" "iface=$IFACE_OVERRIDE"
  $ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH" -i "$IFACE_OVERRIDE"
else
  zereh_log_info "$COMPONENT" "loading generated program using config interface"
  $ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH"
fi

zereh_log_info "$COMPONENT" "production deployment complete" "release_dir=$RELEASE_DIR"
