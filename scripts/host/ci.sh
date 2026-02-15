#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-host}"

"${ROOT_DIR}/scripts/host/build.sh" "${BUILD_DIR}"
python3 -m pip install --upgrade pip
python3 -m pip install websockets
BUILD_DIR="${BUILD_DIR}" "${ROOT_DIR}/scripts/host/smoke.sh"
