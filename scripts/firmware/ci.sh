#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IDF_TARGET="${IDF_TARGET:-esp32s3}"
BUILD_DIR="${BUILD_DIR:-build}"

if [[ -z "${IDF_PATH:-}" ]]; then
  echo "IDF_PATH is not set. Run this from an ESP-IDF shell or source export.sh first." >&2
  exit 1
fi

cd "${ROOT_DIR}"
# shellcheck disable=SC1091
. "${IDF_PATH}/export.sh"

idf.py -B "${BUILD_DIR}" set-target "${IDF_TARGET}"
idf.py -B "${BUILD_DIR}" build
