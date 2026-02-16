#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-host}"
VENV_DIR="${VENV_DIR:-${ROOT_DIR}/.tmp/host-ci-venv}"

"${ROOT_DIR}/scripts/host/build.sh" "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
python3 -m venv "${VENV_DIR}"
"${VENV_DIR}/bin/python" -m pip install --upgrade pip
"${VENV_DIR}/bin/python" -m pip install websockets
PYTHON="${VENV_DIR}/bin/python" BUILD_DIR="${BUILD_DIR}" "${ROOT_DIR}/scripts/host/smoke.sh"
