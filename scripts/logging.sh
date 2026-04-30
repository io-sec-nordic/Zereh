#!/usr/bin/env bash

zereh_log_now_utc() {
  date -u +%Y-%m-%dT%H:%M:%SZ
}

zereh_log_is_structured() {
  [[ "${ZEREH_LOG_MODE:-plain}" == "structured" ]]
}

zereh_log_emit() {
  local level="$1"
  local component="$2"
  local message="$3"
  shift 3

  local ts
  local sanitized

  ts="$(zereh_log_now_utc)"
  sanitized="${message//\"/\'}"

  if zereh_log_is_structured; then
    printf 'ts=%s level=%s component=%s msg="%s"' "$ts" "$level" "$component" "$sanitized"
    for kv in "$@"; do
      printf ' %s' "$kv"
    done
    printf '\n'
    return 0
  fi

  printf '[%s] [%s] %s' "$component" "$level" "$message"
  if [[ $# -gt 0 ]]; then
    printf ' (%s)' "$*"
  fi
  printf '\n'
}

zereh_log_info() {
  zereh_log_emit "info" "$@"
}

zereh_log_warn() {
  zereh_log_emit "warn" "$@"
}

zereh_log_error() {
  zereh_log_emit "error" "$@"
}
