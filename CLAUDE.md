# CLAUDE.md — MimiClaw Operational Playbook

> Authoritative operational contract for AI agents and contributors working on MimiClaw.
> This file is symlinked as `AGENTS.md`.

---

## 1. Mission

MimiClaw is an ESP32-S3 AI agent firmware written in pure C on FreeRTOS. The mission is to run a fully functional AI assistant on a $5 chip with no Linux, no Node.js, and no external dependencies beyond WiFi connectivity.

**Design constraints are non-negotiable:**
- Single-chip deployment (ESP32-S3 with 8MB PSRAM, 16MB flash)
- No dynamic linking, no external runtime
- Memory budget: ~8MB total, ~40KB internal SRAM for stacks
- All configuration via build-time secrets or runtime NVS

---

## 2. Non-Negotiable Rules (MUST/SHOULD)

### Build System

- You **MUST** use ESP-IDF v5.5 or later
- You **MUST** run `idf.py fullclean && idf.py build` after any change to `mimi_secrets.h`
- You **MUST NOT** commit `main/mimi_secrets.h` (gitignored)
- You **SHOULD** use `main/mimi_secrets.h.example` as template

### Memory Management

- You **MUST** allocate buffers >4KB from PSRAM via `heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM)`
- You **MUST** free all heap allocations; ESP32 has no garbage collector
- You **MUST NOT** allocate large buffers on stack (stack size is limited)
- You **SHOULD** check `heap_info` CLI command before/after changes

### Code Style

- You **MUST** use C99 with ESP-IDF conventions
- You **MUST** prefix all public symbols with `mimi_` or module name (e.g., `llm_`, `agent_`)
- You **MUST NOT** use C++ or external C libraries not in ESP-IDF
- You **SHOULD** keep functions under 100 lines

### API Keys and Secrets

- You **MUST NOT** hardcode API keys in source files
- You **MUST** use `mimi_secrets.h` for build-time secrets
- You **MUST** use NVS for runtime secret overrides via CLI
- You **SHOULD** mask secrets in `config_show` output (first 4 chars only)

### Testing

- You **MUST** test on real ESP32-S3 hardware before merging
- You **MUST** verify WiFi connectivity and Telegram message flow
- You **SHOULD** test both Anthropic and OpenAI providers

---

## 3. Fast Start Commands

### Build and Flash

```bash
# Clone and enter
git clone https://github.com/memovai/mimiclaw.git && cd mimiclaw

# Set target
idf.py set-target esp32s3

# Configure secrets
cp main/mimi_secrets.h.example main/mimi_secrets.h
# Edit main/mimi_secrets.h with your credentials

# Build
idf.py fullclean && idf.py build

# Flash (find port: ls /dev/cu.usb* on macOS, ls /dev/ttyACM* on Linux)
idf.py -p /dev/cu.usbmodem11401 flash monitor
```

### Serial CLI Quick Reference

```
mimi> config_show                  # View all configuration
mimi> set_api_key <KEY>            # Set LLM API key
mimi> set_model_provider anthropic # Set provider (anthropic|openai)
mimi> set_model claude-opus-4-5    # Set model identifier
mimi> wifi_set <SSID> <PASS>       # Set WiFi credentials
mimi> restart                      # Apply changes
```

---

## 4. Architecture Truth Map

```
main/
├── mimi.c                  ← app_main() entry point
├── mimi_config.h           ← ALL compile-time constants
├── mimi_secrets.h          ← Build-time credentials (gitignored)
│
├── agent/
│   ├── agent_loop.c        ← ReAct loop (Core 1, priority 6)
│   └── context_builder.c   ← System prompt assembly
│
├── llm/
│   └── llm_proxy.c         ← Dual-provider LLM abstraction
│
├── cli/
│   └── serial_cli.c        ← All CLI commands defined here
│
├── telegram/
│   └── telegram_bot.c      ← Long polling + message dispatch
│
├── gateway/
│   └── ws_server.c         ← WebSocket server on port 18789
│
├── tools/
│   ├── tool_registry.c     ← Tool registration + dispatch
│   └── tool_web_search.c   ← Brave Search API integration
│
├── memory/
│   ├── memory_store.c      ← MEMORY.md + daily notes
│   └── session_mgr.c       ← Per-chat JSONL sessions
│
├── bus/
│   └── message_bus.c       ← FreeRTOS queue abstraction
│
├── proxy/
│   └── http_proxy.c        ← HTTP CONNECT tunnel
│
└── wifi/
    └── wifi_manager.c      ← WiFi STA lifecycle
```

### Core Invariants

| File | Invariant |
|------|-----------|
| `mimi.c:83-146` | Startup sequence order is fixed |
| `agent_loop.c:124` | Max 10 tool iterations per request |
| `llm_proxy.c:83-86` | Provider detection via `strcmp(s_provider, "openai")` |
| `serial_cli.c:327-521` | All CLI commands registered here |
| `mimi_config.h:62-63` | Default model: `claude-opus-4-5`, default provider: `anthropic` |

---

## 5. Memory and Ownership Invariants

### Message Bus Ownership

```c
// Ownership transfers on push — receiver MUST free content
mimi_msg_t msg;
msg.content = strdup("hello");  // caller allocates
message_bus_push_inbound(&msg); // ownership transfers
// ... later in agent_loop ...
free(msg.content);              // receiver frees
```

### Buffer Allocation Rules

| Buffer Type | Location | Size | Allocation |
|-------------|----------|------|------------|
| Task stacks | Internal SRAM | 4-12 KB | Static |
| System prompt | PSRAM | 16 KB | `heap_caps_calloc` |
| LLM response | PSRAM | 32 KB | `heap_caps_calloc` |
| Tool output | PSRAM | 8 KB | `heap_caps_calloc` |
| Session history | PSRAM | 32 KB | `heap_caps_calloc` |

### Session Storage

- Location: `/spiffs/sessions/tg_<chat_id>.jsonl`
- Format: One JSON object per line (`{"role":"...","content":"...","ts":...}`)
- Max messages: 20 (ring buffer, oldest dropped)

---

## 6. LLM Provider Contract (Anthropic vs OpenAI)

### Provider Selection

Source: `main/llm/llm_proxy.c:83-101`

```c
static bool provider_is_openai(void) {
    return strcmp(s_provider, "openai") == 0;
}
```

### API Differences

| Aspect | Anthropic | OpenAI |
|--------|-----------|--------|
| Endpoint | `api.anthropic.com/v1/messages` | `api.openai.com/v1/chat/completions` |
| Auth header | `x-api-key: <key>` | `Authorization: Bearer <key>` |
| System prompt | Top-level `system` field | `messages[0].role = "system"` |
| Tool format | `input_schema` | `parameters` |
| Stop reason | `stop_reason: "tool_use"` | `finish_reason: "tool_calls"` |

### Switching Providers

```
mimi> set_model_provider openai
mimi> set_api_key sk-...
mimi> set_model gpt-4o
mimi> restart
```

---

## 7. CLI Command Contract (authoritative list)

Source: `main/cli/serial_cli.c`

### Runtime Configuration Commands

| Command | Arguments | Description | Line |
|---------|-----------|-------------|------|
| `wifi_set` | `<ssid> <password>` | Set WiFi credentials | 30-41 |
| `set_tg_token` | `<token>` | Set Telegram bot token | 57-67 |
| `set_api_key` | `<key>` | Set LLM API key | 75-85 |
| `set_model` | `<model>` | Set LLM model identifier | 93-103 |
| `set_model_provider` | `<provider>` | Set provider (anthropic\|openai) | 111-121 |
| `set_proxy` | `<host> <port>` | Set HTTP proxy | 206-216 |
| `clear_proxy` | — | Remove proxy config | 219-224 |
| `set_search_key` | `<key>` | Set Brave Search API key | 232-242 |
| `config_show` | — | Display all config (masked) | 285-299 |
| `config_reset` | — | Clear NVS, revert to build defaults | 302-317 |

### Debug and Maintenance Commands

| Command | Arguments | Description | Line |
|---------|-----------|-------------|------|
| `wifi_status` | — | Show WiFi connection status | 44-49 |
| `wifi_scan` | — | Scan and list nearby APs | 245-251 |
| `memory_read` | — | Print MEMORY.md contents | 124-138 |
| `memory_write` | `<content>` | Overwrite MEMORY.md | 146-156 |
| `session_list` | — | List all chat sessions | 159-164 |
| `session_clear` | `<chat_id>` | Delete a session | 172-185 |
| `heap_info` | — | Show heap memory usage | 188-197 |
| `restart` | — | Reboot device | 320-325 |
| `help` | — | List available commands | (esp_console built-in) |

---

## 8. Change Workflow by Subsystem

### Adding a New CLI Command

1. Define arg struct in `serial_cli.c` (if command takes args)
2. Implement `cmd_<name>()` handler function
3. Register in `serial_cli_init()` with `esp_console_cmd_register()`
4. Test via serial monitor

### Adding a New Tool

1. Create `tools/tool_<name>.c` and `.h`
2. Implement tool function: `void tool_<name>(const char *input_json, char *output, size_t output_size)`
3. Register in `tool_registry_init()` with JSON schema
4. Add to `tool_registry_execute()` dispatch

### Changing LLM Provider Logic

1. Modify `llm_proxy.c`
2. Update `provider_is_openai()` or add new provider check
3. Add endpoint/header logic in `llm_http_direct()` and `llm_http_via_proxy()`
4. Update JSON builders: `convert_tools_openai()`, `convert_messages_openai()`
5. Update response parsers: `extract_text_openai()`, `extract_text_anthropic()`

### Modifying Agent Behavior

1. `agent_loop.c` — ReAct loop logic, tool execution
2. `context_builder.c` — System prompt content
3. `mimi_config.h` — Iteration limits, buffer sizes

---

## 9. Validation Checklist

Before submitting changes:

- [ ] `idf.py build` succeeds with no warnings
- [ ] Tested on real ESP32-S3 hardware
- [ ] WiFi connects and obtains IP
- [ ] Telegram bot responds to messages
- [ ] `config_show` displays correct values
- [ ] `heap_info` shows reasonable free memory (>5MB PSRAM)
- [ ] No memory leaks (heap stable after multiple requests)
- [ ] Both Anthropic and OpenAI providers work (if touching `llm_proxy.c`)

### Quick Smoke Test

```bash
# Flash and monitor
idf.py -p PORT flash monitor

# In serial console:
mimi> config_show        # Verify config loaded
mimi> wifi_status        # Verify WiFi connected
mimi> heap_info          # Check memory baseline

# Send Telegram message to bot
# Verify response received
# Check heap_info again (should be similar)
```

---

## 10. Related Docs

| Document | Purpose |
|----------|---------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | System design, module map, task layout |
| [docs/TODO.md](docs/TODO.md) | Feature gap tracker vs Nanobot reference |
| [docs/openclaw-mini-linux-macos.md](docs/openclaw-mini-linux-macos.md) | OpenClaw minimal profile for parity testing |
| [README.md](README.md) | User-facing quick start guide |

### OpenClaw-to-MimiClaw Mapping (compact)

| OpenClaw Concept | MimiClaw Equivalent |
|------------------|---------------------|
| Gateway control plane | Task orchestration in `app_main()` |
| Agent loop/tool cycle | `agent/agent_loop.c` |
| Workspace/state dirs | SPIFFS + NVS split |
| Channel adapters | `telegram/` + `gateway/ws_server.c` |
| Provider abstraction | `llm/llm_proxy.c` |

---

*Last updated: 2026-02-15*
