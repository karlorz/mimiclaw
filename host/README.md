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
brew install cmake pkg-config curl libwebsockets cjson
```

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  libcurl4-openssl-dev libwebsockets-dev libcjson-dev
```

## Build

```bash
cmake -S /Users/karlchow/Desktop/code/mimiclaw/host -B /Users/karlchow/Desktop/code/mimiclaw/build-host
cmake --build /Users/karlchow/Desktop/code/mimiclaw/build-host
```

Binary:
- `/Users/karlchow/Desktop/code/mimiclaw/build-host/mimiclaw-host`

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
