#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-host}"
BIN="${BUILD_DIR}/mimiclaw-host"
STATE_ROOT="${STATE_ROOT:-${ROOT_DIR}/.tmp/mimiclaw-host-live}"
CONFIG_PATH="${CONFIG_PATH:-${STATE_ROOT}/config.json}"
LOG_PATH="${LOG_PATH:-${STATE_ROOT}/host.log}"
WS_BIND="${WS_BIND:-127.0.0.1}"
WS_PORT="${WS_PORT:-18789}"

usage() {
  cat <<'EOF'
Usage: scripts/host/boot-live.sh

Boot the MimiClaw host daemon with a deterministic live state root and log file.

Environment overrides:
  BUILD_DIR    host build directory (default: ./build-host)
  STATE_ROOT   runtime state root (default: ./.tmp/mimiclaw-host-live)
  CONFIG_PATH  config JSON path (default: $STATE_ROOT/config.json)
  LOG_PATH     daemon log path (default: $STATE_ROOT/host.log)
  WS_BIND      websocket bind address (default: 127.0.0.1)
  WS_PORT      websocket port (default: 18789)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ ! -x "${BIN}" ]]; then
  echo "host binary not found: ${BIN}" >&2
  echo "run ./scripts/host/build.sh first or use: make host-build" >&2
  exit 1
fi

if [[ ! -f "${CONFIG_PATH}" ]]; then
  echo "config file not found: ${CONFIG_PATH}" >&2
  echo "create it first (example: cp host/config.json.example ${CONFIG_PATH})" >&2
  exit 2
fi

mkdir -p "$(dirname "${LOG_PATH}")" "${STATE_ROOT}"

echo "booting host daemon"
echo "  config: ${CONFIG_PATH}"
echo "  state : ${STATE_ROOT}"
echo "  ws    : ${WS_BIND}:${WS_PORT}"
echo "  log   : ${LOG_PATH}"

exec "${BIN}" \
  --config "${CONFIG_PATH}" \
  --ws-bind "${WS_BIND}" \
  --ws-port "${WS_PORT}" \
  --state-root "${STATE_ROOT}" \
  2>&1 | tee "${LOG_PATH}"
