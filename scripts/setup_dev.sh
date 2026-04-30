#!/usr/bin/env bash
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
  echo "setup_dev.sh currently supports Debian/Ubuntu (apt-get) only"
  exit 1
fi

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  clang \
  llvm \
  make \
  pkg-config \
  libbpf-dev \
  libyaml-dev \
  libelf-dev \
  zlib1g-dev

echo "development dependencies installed"
