#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-${ROOT_DIR}/build-host}"

cmake -S "${ROOT_DIR}/host" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --parallel
