# MimiClaw Host Runtime (Linux/macOS)

This is the native host daemon for MimiClaw mini service mode.

Current scope:
- WebSocket + Telegram channels
- Shared core logic from `main/` (agent loop, tools, memory/session handling)
- Optional production security profile (WS bearer token + Telegram allowlist)
- Config-listed `SKILL.md` loading (prompt/policy only; no script execution)
- Strict file sandbox via virtual `/spiffs/...` mapping into `~/.mimiclaw`

Out of scope in this phase:
- WiFi manager
- Serial CLI
- OTA manager
- Executable skills or dynamic tool registration

## Prompt Runtime Identity

- Prompt identity is auto-selected by build target. Host binaries (`MIMI_HOST_BUILD`) identify runtime as a Linux/macOS host daemon.
- Tool and memory examples keep `/spiffs/...` paths on host because they are virtual compatibility paths mapped into the host state root.
- In host mode, responses to prompts like "what can you do" should describe host-daemon capabilities and channels, and should not claim to be running on ESP32 hardware.

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

Start from:
- `host/config.json.example` (strict production profile template)

Example config (backward-compatible defaults):

```json
{
  "llm": {
    "api_key": "",
    "model": "claude-opus-4-5",
    "provider": "anthropic",
    "api_base": ""
  },
  "ws": {
    "bind": "127.0.0.1",
    "port": 18789
  },
  "state": {
    "root": "~/.mimiclaw"
  },
  "channels": {
    "telegram_enabled": false
  },
  "telegram": {
    "bot_token": ""
  },
  "security": {
    "ws_require_token": false,
    "ws_token": "",
    "telegram_allowlist": ""
  },
  "skills": {
    "enabled": false,
    "dir": "~/.mimiclaw/skills",
    "max_loaded": 4,
    "list": []
  },
  "timezone": "PST8PDT,M3.2.0,M11.1.0"
}
```

Environment overrides (`.env` in current working directory is auto-loaded):
- `MIMI_API_KEY`
- `AI_API_KEY`
- `MIMI_MODEL`
- `AI_MODEL`
- `MIMI_MODEL_PROVIDER`
- `AI_PROVIDER`
- `MIMI_API_BASE`
- `AI_API_BASE`
- `MIMI_SEARCH_KEY`
- `MIMI_WS_BIND`
- `MIMI_WS_PORT`
- `MIMI_STATE_ROOT`
- `MIMI_TIMEZONE`
- `MIMI_TG_TOKEN`
- `MIMI_CHANNEL_TELEGRAM_ENABLED`
- `MIMI_WS_REQUIRE_TOKEN`
- `MIMI_WS_TOKEN`
- `MIMI_TG_ALLOWLIST`
- `MIMI_SKILLS_ENABLED`
- `MIMI_SKILLS_DIR`
- `MIMI_SKILLS_MAX_LOADED`
- `MIMI_SKILLS_LIST` (comma-separated)
- `MIMI_ENV_FILE` (optional custom `.env` path)

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

macOS live boot/debug entries:

```bash
# Boot with deterministic live paths:
#   state  -> .tmp/mimiclaw-host-live
#   config -> .tmp/mimiclaw-host-live/config.json
#   log    -> .tmp/mimiclaw-host-live/host.log
make host-boot-live

# Debug on macOS with LLDB (auto-runs process)
make host-debug-live-macos

# Follow live log
make host-log-live
```

## Production Security Profile

1. Set `security.ws_require_token=true` and a strong `security.ws_token`.
2. Configure `security.telegram_allowlist` with approved chat/user IDs.
3. Set `channels.telegram_enabled=true` only when `telegram.bot_token` is configured.
4. Keep tokens in environment/secret manager; do not commit them to source control.

WebSocket auth contract when required:
- Handshake must include `Authorization: Bearer <token>`.

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

Run baseline CI-equivalent flow locally (build + ctest + keyless smoke + WS auth deny/allow smoke):

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

## WebSocket Protocol

Inbound frame:
```json
{"type":"message","content":"hello","chat_id":"demo"}
```

Outbound frame:
```json
{"type":"response","content":"...","chat_id":"demo"}
```

When WS auth is enabled, include bearer auth in the handshake:
- `Authorization: Bearer <token>`

## Telegram Ingress Policy

- If `security.telegram_allowlist` is empty, all Telegram senders are accepted.
- If set, only matching chat IDs or user IDs are accepted.
- Non-allowlisted senders are rejected and logged with `allowlist-deny` markers.

## Skills Contract

- Load only config-listed `SKILL.md` files from configured `skills.dir`.
- Parse frontmatter keys: `name`, `description`, `required_tools`.
- Skip invalid/missing skills or skills requiring unavailable tools.
- Inject loaded skill summaries/instructions into the system prompt.
- No script execution, shelling out, or dynamic tool registration.

## Sandbox Mapping

Virtual `/spiffs` paths are translated as:
- `/spiffs/config/*` -> `~/.mimiclaw/config/*`
- `/spiffs/memory/*` -> `~/.mimiclaw/memory/*`
- `/spiffs/sessions/*` -> `~/.mimiclaw/sessions/*`

Rules:
- `..` traversal is rejected
- Non-`/spiffs/...` paths are rejected
- Host tooling keeps virtual paths in tool responses
