#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
  python3-venv \
  build-essential cmake pkg-config \
  libcurl4-openssl-dev libwebsockets-dev libcjson-dev
