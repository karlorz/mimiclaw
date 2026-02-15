# CLAUDE.md

## Mission
MimiClaw contributors and agents MUST preserve a reliable, source-true embedded AI runtime on ESP32-S3, with changes grounded in current C code under `main/` and validated before merge.

## Non-Negotiable Rules (MUST/SHOULD)
- You MUST treat source files in `main/` as authoritative over prose docs when they conflict.
- You MUST keep behavior consistent with ESP-IDF v5.5+ build and flash workflow.
- You MUST preserve runtime configuration semantics: NVS overrides build-time defaults where implemented.
- You MUST NOT document or expose CLI commands that are not registered in `main/cli/serial_cli.c`.
- You MUST preserve message ownership rules on `mimi_msg_t` content pointers.
- You MUST keep provider behavior aligned with `main/llm/llm_proxy.c` (including OpenAI vs Anthropic branching).
- You SHOULD keep features off by default unless explicitly required (single-agent, single-channel operating posture).
- You SHOULD update `README.md`, `docs/ARCHITECTURE.md`, and this file when operational behavior changes.

## Fast Start Commands
ESP-IDF baseline and firmware workflow:

```bash
# Prerequisite: ESP-IDF v5.5+ installed
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

idf.py set-target esp32s3
cp main/mimi_secrets.h.example main/mimi_secrets.h

# Required after mimi_secrets.h changes
idf.py fullclean && idf.py build

# Flash + serial monitor
idf.py -p PORT flash monitor
```

Runtime CLI bootstrap (serial console):

```text
wifi_set <ssid> <password>
set_tg_token <token>
set_api_key <key>
set_model_provider <anthropic|openai>
set_model <model>
set_search_key <key>
set_proxy <host> <port>   # optional
config_show
```

## Architecture Truth Map
Authoritative subsystem map:

| Concern | Source of Truth | Contract |
|---|---|---|
| Startup/task orchestration | `main/mimi.c` | `app_main()` initializes subsystems, starts CLI first, then WiFi-dependent services. |
| Agent/tool execution loop | `main/agent/agent_loop.c` | ReAct loop with max `MIMI_AGENT_MAX_TOOL_ITER` iterations and tool execution cycle. |
| Channel ingress/egress | `main/telegram/telegram_bot.c`, `main/gateway/ws_server.c`, `main/mimi.c` | Telegram + WebSocket feed inbound queue; outbound dispatch routes by `channel`. |
| Bus contract | `main/bus/message_bus.h`, `main/bus/message_bus.c` | Two queues (inbound/outbound), ownership transfer of `content` pointer. |
| Persistence split | `main/memory/*.c`, `main/mimi_config.h` | SPIFFS stores memory/session files; NVS stores runtime overrides and credentials. |
| Runtime config commands | `main/cli/serial_cli.c` | CLI writes NVS overrides and exposes diagnostics/maintenance commands. |
| Provider abstraction | `main/llm/llm_proxy.c` | Provider string switches request format, host, path, headers, and response parsing. |

OpenClaw to MimiClaw mapping (mandatory reference):

| OpenClaw Concept | MimiClaw Equivalent |
|---|---|
| Gateway control plane | task orchestration in `app_main()` |
| Agent loop/tool cycle | `agent/agent_loop.c` |
| Workspace/state dirs | SPIFFS + NVS split |
| Channel adapters | `telegram/` + `gateway/ws_server.c` |
| Provider abstraction | `llm/llm_proxy.c` |

## Memory and Ownership Invariants
- `mimi_msg_t.content` MUST be heap-allocated before queue push and MUST be freed by the queue consumer.
- Inbound flow MUST remain channel -> inbound queue -> agent loop.
- Outbound flow MUST remain agent loop -> outbound queue -> channel dispatcher.
- Session persistence MUST continue writing only user and final assistant text via `session_append(...)`.
- SPIFFS MUST remain the store for conversational/state files (`/spiffs/config`, `/spiffs/memory`, `/spiffs/sessions`).
- NVS MUST remain the runtime override store for WiFi, Telegram token, LLM config, proxy, and search key namespaces.

## LLM Provider Contract (Anthropic vs OpenAI)
- Provider state is stored in `s_provider` and persisted by `llm_set_provider(...)` (called by CLI `set_model_provider`).
- Provider selection rule is strict: only exact `"openai"` activates OpenAI mode; every other value follows Anthropic mode.
- `llm_proxy_init()` loads defaults from build-time secrets, then applies NVS overrides with highest priority.
- OpenAI mode contract:
  - URL/path/host: OpenAI endpoint (`/v1/chat/completions`, `api.openai.com`).
  - Auth header: `Authorization: Bearer <key>`.
  - Message/tool payload is converted to OpenAI schema via conversion helpers.
  - Response text is extracted from `choices[0].message.content`.
- Anthropic mode contract:
  - URL/path/host: Anthropic endpoint (`/v1/messages`, `api.anthropic.com`).
  - Auth headers: `x-api-key` and `anthropic-version`.
  - `system` is top-level; messages follow Anthropic content block format.
  - Response text/tool blocks are parsed from `content[]` with `stop_reason` handling.
- Proxy behavior MUST stay provider-aware in both direct and CONNECT-tunnel paths.

## CLI Command Contract (authoritative list)
Only the following commands are authoritative (from `main/cli/serial_cli.c` plus built-in `help`):

| Command | Parameters | Contract |
|---|---|---|
| `help` | none | Show registered commands. |
| `wifi_set` | `<ssid> <password>` | Persist WiFi credentials to NVS. |
| `wifi_status` | none | Show connectivity and current IP. |
| `wifi_scan` | none | Scan and print nearby APs. |
| `set_tg_token` | `<token>` | Persist Telegram bot token to NVS. |
| `set_api_key` | `<key>` | Persist LLM API key to NVS. |
| `set_model` | `<model>` | Persist model identifier to NVS. |
| `set_model_provider` | `<provider>` | Persist provider string to NVS (`openai` exact-match for OpenAI behavior). |
| `set_search_key` | `<key>` | Persist Brave Search key to NVS. |
| `set_proxy` | `<host> <port>` | Persist HTTP proxy host/port to NVS. |
| `clear_proxy` | none | Remove proxy host/port from NVS. |
| `config_show` | none | Print current config values with source labels and masking. |
| `config_reset` | none | Erase all app config namespaces in NVS. |
| `memory_read` | none | Print long-term `MEMORY.md` contents. |
| `memory_write` | `<content>` | Overwrite long-term `MEMORY.md`. |
| `session_list` | none | List persisted sessions. |
| `session_clear` | `<chat_id>` | Delete one session history file. |
| `heap_info` | none | Print internal/PSRAM/total free heap. |
| `restart` | none | Reboot device. |

## Change Workflow by Subsystem
1. CLI changes:
   - Edit `main/cli/serial_cli.c`.
   - Register command + argtable + help text together.
   - Update this file and `README.md` command docs in the same change.
2. LLM/provider changes:
   - Edit `main/llm/llm_proxy.c` and `main/llm/llm_proxy.h`.
   - Validate both providers for request build, auth headers, response parse, and tool path.
   - Reconfirm `set_model_provider` semantics are unchanged or update all docs/tests accordingly.
3. Agent loop/tooling changes:
   - Edit `main/agent/agent_loop.c`, `main/tools/*`, `main/agent/context_builder.c`.
   - Preserve ReAct iteration guard (`MIMI_AGENT_MAX_TOOL_ITER`) unless intentionally changed.
   - Preserve queue ownership and session append behavior.
4. Persistence/config changes:
   - Edit `main/memory/*`, `main/mimi_config.h`, and relevant modules using NVS namespaces.
   - Keep SPIFFS vs NVS responsibilities explicit and non-overlapping.
5. Channel/gateway changes:
   - Edit `main/telegram/*`, `main/gateway/ws_server.c`, and outbound routing in `main/mimi.c`.
   - Preserve channel identifiers and bus routing semantics.

## Validation Checklist
- `idf.py build` completes on ESP-IDF v5.5+ toolchain.
- CLI list in docs matches actual registrations in `main/cli/serial_cli.c`.
- Provider claims match `main/llm/llm_proxy.c` exact branching behavior.
- Queue ownership invariants remain true (`message_bus.h` contract still valid).
- Startup/task ordering remains accurate with `main/mimi.c`.
- OpenClaw mini-profile defaults remain coherent with `docs/openclaw-mini-linux-macos.md`:
  - single gateway,
  - single agent/workspace,
  - one ingress channel,
  - token auth on,
  - persistent state on,
  - browser off by default,
  - no cron/multi-agent by default.

## Related Docs
- `docs/openclaw-mini-linux-macos.md`
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/TODO.md`
- `main/cli/serial_cli.c`
- `main/llm/llm_proxy.c`
