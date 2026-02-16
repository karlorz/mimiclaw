# MimiClaw Host Runtime (Linux/macOS)

This is the phase-1 native host daemon for MimiClaw.

Scope in this phase:
- WebSocket ingress only
- Shared core logic from `main/` (agent loop, tools, memory/session handling)
- Strict file sandbox via virtual `/spiffs/...` mapping into `~/.mimiclaw`

Out of scope in this phase:
- WiFi manager
- Serial CLI
- Telegram poller/sender
- OTA manager

## Dependencies

### macOS (Homebrew)

```bash
./scripts/host/install-deps-macos.sh
```

### Ubuntu/Debian

```bash
./scripts/host/install-deps-ubuntu.sh
```

## Build

```bash
./scripts/host/build.sh
# or
make host-build
```

Binary:
- `/Users/karlchow/Desktop/code/mimiclaw/build-host/mimiclaw-host`

If this workspace was synced from another machine, clear stale host artifacts before rebuilding to avoid CMake source-path mismatch errors:

```bash
rm -rf build-host .tmp/host-ci-venv
```

## Config

Default config file:
- `~/.mimiclaw/config.json`

Example:

```json
{
  "api_key": "sk-ant-api03-...",
  "model": "claude-opus-4-5",
  "model_provider": "anthropic",
  "search_key": "BSA...",
  "ws_bind": "127.0.0.1",
  "ws_port": 18789,
  "state_root": "~/.mimiclaw",
  "timezone": "PST8PDT,M3.2.0,M11.1.0"
}
```

You can also start from:
- `host/config.json.example`

Environment overrides:
- `MIMI_API_KEY`
- `MIMI_MODEL`
- `MIMI_MODEL_PROVIDER`
- `MIMI_SEARCH_KEY`
- `MIMI_WS_BIND`
- `MIMI_WS_PORT`
- `MIMI_STATE_ROOT`
- `MIMI_TIMEZONE`

CLI flags:
- `--config <path>`
- `--ws-bind <addr>`
- `--ws-port <port>`
- `--state-root <path>`

Precedence:
1. config file
2. environment
3. CLI flags

## Run

```bash
/Users/karlchow/Desktop/code/mimiclaw/build-host/mimiclaw-host
```

Or with overrides:

```bash
/Users/karlchow/Desktop/code/mimiclaw/build-host/mimiclaw-host \
  --config ~/.mimiclaw/config.json \
  --ws-bind 127.0.0.1 \
  --ws-port 18789 \
  --state-root ~/.mimiclaw
```

## Validation Flows (Reusable for CI)

Install Python test dependency:

```bash
python3 -m venv .tmp/host-venv
. .tmp/host-venv/bin/activate
python -m pip install websockets
```

Run baseline smoke (valid WS flow + malformed/non-message payload robustness):

```bash
./scripts/host/smoke.sh
# or
make host-smoke
```

Run baseline CI-equivalent flow locally (build + host regression tests + keyless smoke):

```bash
./scripts/host/ci.sh
# or
make host-ci
```

Run required live-provider checks:

```bash
export ANTHROPIC_API_KEY=...
export OPENAI_API_KEY=...

make host-ci-live-anthropic
make host-ci-live-openai
# or run both sequentially
make host-ci-live-all
```

Failure log locations:
- `/Users/karlchow/Desktop/code/mimiclaw/.tmp/mimiclaw-host-smoke/host.log`
- `/Users/karlchow/Desktop/code/mimiclaw/.tmp/mimiclaw-host-smoke/config.json`
- `/Users/karlchow/Desktop/code/mimiclaw/.tmp/mimiclaw-host-smoke-live-anthropic/host.log`
- `/Users/karlchow/Desktop/code/mimiclaw/.tmp/mimiclaw-host-smoke-live-openai/host.log`

## cloudrouter e2b Runbook

Minimum Linux sequence for reproducible host readiness in a fresh sandbox:

```bash
cloudrouter start . -p e2b
./scripts/host/install-deps-ubuntu.sh
rm -rf build-host .tmp/host-ci-venv .tmp/mimiclaw-host-smoke .tmp/mimiclaw-host-smoke-live-*

make host-ci

# Required live validation
export ANTHROPIC_API_KEY=...
export OPENAI_API_KEY=...
make host-ci-live-all
```

## Common Failures And Fixes

- CMake cache source mismatch (`The source ... does not match the source ... used to generate cache`):
  `rm -rf build-host .tmp/host-ci-venv`
- Missing `python3 -m venv` support:
  rerun `./scripts/host/install-deps-ubuntu.sh` (installs `python3-venv`)
- Live validation exits for missing API keys:
  set `ANTHROPIC_API_KEY` and/or `OPENAI_API_KEY` before running `make host-ci-live-*`

## WebSocket Protocol (phase-1)

Inbound frame:
```json
{"type":"message","content":"hello","chat_id":"demo"}
```

Outbound frame:
```json
{"type":"response","content":"...","chat_id":"demo"}
```

## Sandbox Mapping

Virtual `/spiffs` paths are translated as:
- `/spiffs/config/*` -> `~/.mimiclaw/config/*`
- `/spiffs/memory/*` -> `~/.mimiclaw/memory/*`
- `/spiffs/sessions/*` -> `~/.mimiclaw/sessions/*`

Rules:
- `..` traversal is rejected
- Non-`/spiffs/...` paths are rejected
- Host tooling keeps virtual paths in tool responses
