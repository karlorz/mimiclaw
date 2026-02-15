#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-host}"
BIN="${BUILD_DIR}/mimiclaw-host"

if [[ ! -x "${BIN}" ]]; then
  echo "host binary not found: ${BIN}" >&2
  echo "run scripts/host/build.sh first" >&2
  exit 1
fi

if ! python3 -c 'import websockets' >/dev/null 2>&1; then
  echo "python package 'websockets' not installed" >&2
  echo "install: python3 -m pip install websockets" >&2
  exit 2
fi

pick_port() {
  python3 - <<'PY'
import socket
s = socket.socket()
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PY
}

wait_port() {
  local host="$1"
  local port="$2"
  python3 - "$host" "$port" <<'PY'
import socket
import sys
import time

host = sys.argv[1]
port = int(sys.argv[2])
deadline = time.time() + 15

while time.time() < deadline:
    s = socket.socket()
    s.settimeout(0.5)
    try:
        s.connect((host, port))
        s.close()
        sys.exit(0)
    except Exception:
        time.sleep(0.1)
    finally:
        try:
            s.close()
        except Exception:
            pass

sys.exit(1)
PY
}

STATE_ROOT="${STATE_ROOT:-${ROOT_DIR}/.tmp/mimiclaw-host-smoke}"
WS_BIND="${WS_BIND:-127.0.0.1}"
WS_PORT="${WS_PORT:-$(pick_port)}"
CONFIG_PATH="${STATE_ROOT}/config.json"
LOG_PATH="${STATE_ROOT}/host.log"

mkdir -p "${STATE_ROOT}/config" "${STATE_ROOT}/memory" "${STATE_ROOT}/sessions"

cat > "${CONFIG_PATH}" <<JSON
{
  "api_key": "",
  "model": "claude-opus-4-5",
  "model_provider": "anthropic",
  "search_key": "",
  "ws_bind": "${WS_BIND}",
  "ws_port": ${WS_PORT},
  "state_root": "${STATE_ROOT}",
  "timezone": "PST8PDT,M3.2.0,M11.1.0"
}
JSON

cleanup() {
  local rc=$?
  if [[ -n "${HOST_PID:-}" ]] && kill -0 "${HOST_PID}" >/dev/null 2>&1; then
    kill "${HOST_PID}" >/dev/null 2>&1 || true
    wait "${HOST_PID}" >/dev/null 2>&1 || true
  fi
  exit $rc
}
trap cleanup EXIT

"${BIN}" \
  --config "${CONFIG_PATH}" \
  --ws-bind "${WS_BIND}" \
  --ws-port "${WS_PORT}" \
  --state-root "${STATE_ROOT}" \
  >"${LOG_PATH}" 2>&1 &
HOST_PID=$!

if ! wait_port "${WS_BIND}" "${WS_PORT}"; then
  echo "host daemon did not start in time" >&2
  tail -n 120 "${LOG_PATH}" >&2 || true
  exit 1
fi

python3 "${ROOT_DIR}/scripts/host/smoke_ws.py" \
  --url "ws://${WS_BIND}:${WS_PORT}" \
  --chat-id "ci_smoke" \
  --content "hello from smoke" \
  --timeout 20

if [[ ! -f "${STATE_ROOT}/sessions/tg_ci_smoke.jsonl" ]]; then
  echo "expected session file missing: ${STATE_ROOT}/sessions/tg_ci_smoke.jsonl" >&2
  exit 1
fi

echo "host smoke passed (ws=${WS_BIND}:${WS_PORT})"
