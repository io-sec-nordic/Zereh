#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:-build/generated}"
VERSION_IN="${2:-}"
REV_IN="${3:-}"

if [[ -z "$VERSION_IN" ]]; then
  if [[ -f VERSION ]]; then
    VERSION_IN="$(tr -d '[:space:]' < VERSION)"
  else
    VERSION_IN="0.1.0-dev"
  fi
fi

if [[ -z "$REV_IN" ]]; then
  if git rev-parse --short=12 HEAD >/dev/null 2>&1; then
    REV_IN="$(git rev-parse --short=12 HEAD)"
  else
    REV_IN="nogit"
  fi
fi

BUILD_TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

mkdir -p "$OUT_DIR"

cat > "$OUT_DIR/zereh_build_info.h" <<EOF
#ifndef ZEREH_BUILD_INFO_H
#define ZEREH_BUILD_INFO_H

#define ZEREH_BUILD_VERSION "${VERSION_IN}"
#define ZEREH_BUILD_REVISION "${REV_IN}"
#define ZEREH_BUILD_TIMESTAMP_UTC "${BUILD_TS}"

#endif /* ZEREH_BUILD_INFO_H */
EOF

cat > "$OUT_DIR/zereh_license.txt" <<EOF
Zereh Build License Metadata
============================

Project: Zereh
Version: ${VERSION_IN}
Revision: ${REV_IN}
Build Timestamp (UTC): ${BUILD_TS}

SPDX: AGPL-3.0-or-later
Commercial Holder: IO-SEC Nordic AB
Commercial Contact: commercial-license@io-sec.eu
EOF

echo "[meta] wrote $OUT_DIR/zereh_build_info.h"
echo "[meta] wrote $OUT_DIR/zereh_license.txt"
