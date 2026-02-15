# CLAUDE.md

## Mission
MimiClaw is an ESP32-S3 AI agent firmware written in pure C on FreeRTOS. This document is the operational contract for contributors and coding agents and MUST be treated as the first entry point before modifying code.

## Non-Negotiable Rules (MUST/SHOULD)
- You MUST treat `main/` implementation files as source of truth when prose docs conflict.
- You MUST preserve runtime config precedence used by active modules: NVS overrides (set via CLI) take priority; build-time secrets are fallback defaults.
- You MUST NOT commit `main/mimi_secrets.h`; use `main/mimi_secrets.h.example` as template.
- You MUST preserve message-bus ownership semantics: push transfers `content` ownership, pop requires receiver to `free()`.
- You MUST keep large buffers on PSRAM (`heap_caps_calloc(..., MALLOC_CAP_SPIRAM)`), not stack/internal SRAM.
- You MUST preserve core assignment intent: I/O and channel tasks on Core 0, agent compute loop on Core 1.
- You MUST keep provider-switch behavior correct for both Anthropic and OpenAI code paths.
- You SHOULD keep changes subsystem-local and update docs whenever command contracts or runtime behavior change.

## Fast Start Commands
Requires ESP-IDF v5.5+ (CI uses v5.5.2).

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
idf.py set-target esp32s3
idf.py build
idf.py fullclean && idf.py build
idf.py -p PORT flash monitor
```

Operational notes:
- Run `idf.py fullclean && idf.py build` after changing `main/mimi_secrets.h`.
- No unit-test/lint framework is configured; validation is build + device smoke checks via serial CLI and channel messaging.

## Architecture Truth Map
Entrypoint and orchestration:
- `main/mimi.c:app_main()` initializes NVS, event loop, SPIFFS, then subsystems.
- Serial CLI starts before WiFi so debugging works even without network.
- On successful WiFi connect, Telegram poller, agent loop, WebSocket server, and outbound dispatcher start.

Required OpenClaw-to-MimiClaw mapping:

| OpenClaw concept | MimiClaw equivalent |
| --- | --- |
| Gateway control plane | Task orchestration in `app_main()` |
| Agent loop/tool cycle | `agent/agent_loop.c` |
| Workspace/state dirs | SPIFFS + NVS split |
| Channel adapters | `telegram/` + `gateway/ws_server.c` |
| Provider abstraction | `llm/llm_proxy.c` |

Primary module truth map:
- `main/bus/` queue transport and message ownership boundaries.
- `main/agent/` ReAct loop and context assembly.
- `main/llm/` provider adapter and HTTP request shaping.
- `main/tools/` tool registry and built-in tools (`web_search`, `get_current_time`, `read_file`, `write_file`, `edit_file`, `list_dir`).
- `main/memory/` long-term memory and session persistence.
- `main/telegram/` Telegram polling/send channel adapter.
- `main/gateway/` WebSocket channel adapter.
- `main/proxy/` HTTP CONNECT path.
- `main/cli/` command registration and runtime override surface.

## Memory and Ownership Invariants
Message bus contract (`main/bus/message_bus.h`):

```c
typedef struct {
    char channel[16];
    char chat_id[32];
    char *content;  // heap allocated; ownership transferred on queue push
} mimi_msg_t;
```

Invariants:
- `message_bus_push_inbound()` and `message_bus_push_outbound()` take ownership of `msg->content`.
- `message_bus_pop_inbound()` and `message_bus_pop_outbound()` return ownership to caller; caller MUST `free(msg->content)`.
- Queue depth is fixed (`MIMI_BUS_QUEUE_LEN = 8`); do not assume unbounded buffering.
- Agent loop allocates major buffers from PSRAM (`system_prompt`, `history_json`, tool output).
- SPIFFS is flat; path hierarchy is naming convention only.
- Persistent storage paths are `/spiffs/config/SOUL.md`, `/spiffs/config/USER.md`, `/spiffs/memory/MEMORY.md`, `/spiffs/memory/YYYY-MM-DD.md`, and `/spiffs/sessions/tg_<chat_id>.jsonl`.

## LLM Provider Contract (Anthropic vs OpenAI)
Provider selection and precedence (`main/llm/llm_proxy.c`):
- Default provider is `anthropic` via `MIMI_LLM_PROVIDER_DEFAULT`.
- Build-time provider can be set by `MIMI_SECRET_MODEL_PROVIDER`.
- NVS override (`set_model_provider`) has highest runtime priority.

Request contract:
- Anthropic path uses `https://api.anthropic.com/v1/messages`.
- Anthropic sets `x-api-key` and `anthropic-version` headers.
- Anthropic sends system prompt as top-level `system` field.
- OpenAI path uses `https://api.openai.com/v1/chat/completions`.
- OpenAI sets `Authorization: Bearer <key>` header.
- OpenAI injects system prompt as `messages` entry with role `system`.
- Tool schemas are converted from Anthropic-style tool JSON to OpenAI function-tool format.

Tool-call stop semantics:
- Anthropic tool execution round is detected by `stop_reason == "tool_use"`.
- OpenAI tool execution round is detected by `finish_reason == "tool_calls"` and/or parsed `tool_calls` array.

Shared behavior:
- `llm_chat()` and `llm_chat_tools()` are non-streaming.
- Both direct HTTPS and proxy CONNECT routes must remain functional.

## CLI Command Contract (authoritative list)
Source of truth: command registration in `main/cli/serial_cli.c`.

General:
- `help`
- `restart`
- `heap_info`

WiFi:
- `wifi_set <ssid> <password>`
- `wifi_status`
- `wifi_scan`

Telegram/LLM/Search runtime config:
- `set_tg_token <token>`
- `set_api_key <key>`
- `set_model <model>`
- `set_model_provider <provider>` where provider is `anthropic|openai`
- `set_search_key <key>`

Proxy/config management:
- `set_proxy <host> <port>`
- `clear_proxy`
- `config_show`
- `config_reset`

Memory/session:
- `memory_read`
- `memory_write <content>`
- `session_list`
- `session_clear <chat_id>`

## Change Workflow by Subsystem
- Bus/routing changes (`main/bus/*`, `main/mimi.c`): validate ownership and dispatch behavior for Telegram and WebSocket.
- Agent/context changes (`main/agent/*`): validate ReAct loop iteration behavior, tool-result message shape, and session append logic.
- LLM changes (`main/llm/llm_proxy.c`): validate Anthropic/OpenAI parity including endpoint, headers, tool conversion, and stop-condition parsing.
- Tooling changes (`main/tools/*`): validate registry JSON generation and execution dispatch for all registered tools.
- Telegram channel changes (`main/telegram/*`): validate polling stability and send fallback behavior.
- Memory/session changes (`main/memory/*`): validate SPIFFS file path behavior and bounded history assumptions.
- CLI/config changes (`main/cli/*`): validate command registration, argument parsing, and NVS override side effects.

## Validation Checklist
- Build check: `idf.py build`.
- Secrets-change build check: `idf.py fullclean && idf.py build` after any `main/mimi_secrets.h` change.
- CLI contract check: `help` shows expected commands; `config_show` output matches expected effective values.
- Provider check: switch provider with `set_model_provider`, then verify logs and successful response path.
- Tool loop check: run at least one query that triggers tool use and final assistant response.
- Queue/ownership check: verify no crashes or leaks under repeated inbound/outbound message flow.

## Related Docs
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/TODO.md`
- `docs/openclaw-mini-linux-macos.md`
