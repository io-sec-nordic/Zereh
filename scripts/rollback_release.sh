#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/logging.sh"

cd "$REPO_ROOT"

RELEASE_SELECTOR="${1:-previous}"
IFACE_OVERRIDE="${2:-}"
BASE_DIR="build/release"
COMPONENT="rollback"

resolve_release_dir() {
  local selector="$1"
  local dirs=()

  if [[ -d "$selector" ]]; then
    printf "%s\n" "$selector"
    return 0
  fi

  if [[ -d "$BASE_DIR/$selector" ]]; then
    printf "%s\n" "$BASE_DIR/$selector"
    return 0
  fi

  mapfile -t dirs < <(find "$BASE_DIR" -mindepth 1 -maxdepth 1 -type d | sort)
  if [[ ${#dirs[@]} -eq 0 ]]; then
    zereh_log_error "$COMPONENT" "no release directories found" "base_dir=$BASE_DIR"
    return 1
  fi

  case "$selector" in
    latest)
      printf "%s\n" "${dirs[${#dirs[@]}-1]}"
      ;;
    previous)
      if [[ ${#dirs[@]} -lt 2 ]]; then
        zereh_log_error "$COMPONENT" "selector previous requires at least two releases" "base_dir=$BASE_DIR"
        return 1
      fi
      printf "%s\n" "${dirs[${#dirs[@]}-2]}"
      ;;
    *)
      zereh_log_error "$COMPONENT" "invalid release selector" "selector=$selector"
      return 1
      ;;
  esac
}

RELEASE_DIR="$(resolve_release_dir "$RELEASE_SELECTOR")"
CONFIG_PATH="$RELEASE_DIR/config.yaml"

if [[ ! -f "$CONFIG_PATH" ]]; then
  zereh_log_error "$COMPONENT" "release config not found" "config=$CONFIG_PATH"
  exit 1
fi

if [[ ! -x ./build/bin/zerehctl ]]; then
  zereh_log_warn "$COMPONENT" "zerehctl not found; building binaries"
  make -j"$(nproc)"
fi

ROOT_CMD=""
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  ROOT_CMD="sudo"
fi

zereh_log_info "$COMPONENT" "regenerating object from rollback config" "release_dir=$RELEASE_DIR"
./build/bin/zerehctl generate -c "$CONFIG_PATH"

if [[ -n "$IFACE_OVERRIDE" ]]; then
  zereh_log_info "$COMPONENT" "loading rollback config on explicit interface" "iface=$IFACE_OVERRIDE"
  $ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH" -i "$IFACE_OVERRIDE"
else
  zereh_log_info "$COMPONENT" "loading rollback config using interface from config"
  $ROOT_CMD ./build/bin/zerehctl load -c "$CONFIG_PATH"
fi

zereh_log_info "$COMPONENT" "rollback completed" "release_dir=$RELEASE_DIR"
