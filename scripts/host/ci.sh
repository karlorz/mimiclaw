#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-host}"
VENV_DIR="${VENV_DIR:-${ROOT_DIR}/.tmp/host-ci-venv}"
LIVE_PROVIDER="${LIVE_PROVIDER:-}"

usage() {
  cat <<'EOF'
Usage: scripts/host/ci.sh [--live-provider anthropic|openai]

Default mode:
  Baseline host CI (build + ctest + keyless smoke)

Live mode:
  Build + ctest + live provider smoke (requires API key env)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --live-provider)
      if [[ $# -lt 2 ]]; then
        echo "missing value for --live-provider" >&2
        exit 2
      fi
      LIVE_PROVIDER="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

KEY_NAME=""
LIVE_API_KEY=""
LIVE_MODEL=""
LIVE_PROVIDER_NORM=""

if [[ -n "${LIVE_PROVIDER}" ]]; then
  LIVE_PROVIDER_NORM="$(printf '%s' "${LIVE_PROVIDER}" | tr '[:upper:]' '[:lower:]')"

  case "${LIVE_PROVIDER_NORM}" in
    anthropic)
      KEY_NAME="ANTHROPIC_API_KEY"
      LIVE_API_KEY="${ANTHROPIC_API_KEY:-${MIMI_API_KEY:-}}"
      LIVE_MODEL="${LIVE_MODEL_ANTHROPIC:-${MIMI_MODEL:-claude-opus-4-5}}"
      ;;
    openai)
      KEY_NAME="OPENAI_API_KEY"
      LIVE_API_KEY="${OPENAI_API_KEY:-${MIMI_API_KEY:-}}"
      LIVE_MODEL="${LIVE_MODEL_OPENAI:-${MIMI_MODEL:-gpt-4o-mini}}"
      ;;
    *)
      echo "invalid live provider: ${LIVE_PROVIDER} (expected anthropic|openai)" >&2
      exit 2
      ;;
  esac

  if [[ -z "${LIVE_API_KEY}" ]]; then
    echo "missing API key for live provider '${LIVE_PROVIDER_NORM}'" >&2
    echo "set ${KEY_NAME} (or MIMI_API_KEY) before running live validation" >&2
    exit 2
  fi
fi

"${ROOT_DIR}/scripts/host/build.sh" "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
python3 -m venv "${VENV_DIR}"
"${VENV_DIR}/bin/python" -m pip install --upgrade pip
"${VENV_DIR}/bin/python" -m pip install websockets

if [[ -z "${LIVE_PROVIDER}" ]]; then
  echo "running baseline host smoke (keyless deterministic mode)"
  PYTHON="${VENV_DIR}/bin/python" BUILD_DIR="${BUILD_DIR}" "${ROOT_DIR}/scripts/host/smoke.sh"
  exit 0
fi

echo "running live host smoke (provider=${LIVE_PROVIDER_NORM}, model=${LIVE_MODEL})"
PYTHON="${VENV_DIR}/bin/python" \
BUILD_DIR="${BUILD_DIR}" \
STATE_ROOT="${STATE_ROOT:-${ROOT_DIR}/.tmp/mimiclaw-host-smoke-live-${LIVE_PROVIDER_NORM}}" \
SMOKE_CLIENT="${ROOT_DIR}/scripts/host/smoke_ws_live.py" \
SMOKE_CHAT_ID="${SMOKE_CHAT_ID:-ci_live_${LIVE_PROVIDER_NORM}}" \
SMOKE_CONTENT="${SMOKE_CONTENT:-Reply with a short live validation acknowledgement.}" \
SMOKE_TIMEOUT="${SMOKE_TIMEOUT:-45}" \
SMOKE_DISALLOW_FALLBACK="${SMOKE_DISALLOW_FALLBACK:-Sorry, I encountered an error.}" \
LLM_API_KEY="${LIVE_API_KEY}" \
LLM_MODEL_PROVIDER="${LIVE_PROVIDER_NORM}" \
LLM_MODEL="${LIVE_MODEL}" \
"${ROOT_DIR}/scripts/host/smoke.sh"
