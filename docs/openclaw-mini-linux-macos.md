# OpenClaw Minimal Profile for Linux/macOS

> A source-backed deep dive into running a minimal OpenClaw deployment on Linux/macOS, with explicit mappings to MimiClaw embedded concepts.

---

## 1. Purpose and Scope

This document defines the **minimal viable OpenClaw configuration** for Linux/macOS that mirrors MimiClaw's embedded design constraints. Use this as a reference when:

- Testing MimiClaw behavior against a desktop OpenClaw instance
- Understanding which OpenClaw features map to embedded equivalents
- Validating agent logic before flashing to ESP32-S3

This document does NOT cover OpenClaw's full feature set. For complete documentation, see the [OpenClaw repository](https://github.com/openclaw/openclaw).

---

## 2. What "Minimal OpenClaw" Means

A minimal OpenClaw deployment strips away features that exceed embedded hardware capabilities:

| Full OpenClaw | Minimal Profile | Rationale |
|---------------|-----------------|-----------|
| Multi-agent routing | Single agent | ESP32 memory limits |
| Cron scheduler | Disabled | No persistent timer service on embedded |
| Voice/talk channels | Disabled | No audio I/O on base ESP32-S3 |
| Live canvas | Disabled | No display output |
| Browser automation | Disabled by default | Resource-intensive |
| Multiple simultaneous channels | One channel at a time | Memory constraints |

The minimal profile targets **functional parity with MimiClaw**, not feature completeness.

---

## 3. Smallest Viable Runtime Components

A minimal OpenClaw deployment requires exactly:

| Component | Description | Required |
|-----------|-------------|----------|
| Gateway process | Core control plane | YES |
| Single agent (`main`) | Executes ReAct loop | YES |
| One workspace | State directory | YES |
| One ingress channel | Telegram OR WebSocket | YES |
| Token auth | API key validation | YES |
| Persistent state dir | Session/memory storage | YES |

**Disabled by default in mini profile:**
- Browser node
- Cron service
- Multi-agent orchestration
- Additional channels beyond primary

---

## 4. Minimal Config Baseline (from official examples)

### Installation

```bash
curl -fsSL --proto '=https' --tlsv1.2 https://openclaw.ai/install.sh | bash
```

### Start Gateway

```bash
openclaw gateway --port 18789
```

### Config Location

```
~/.openclaw/openclaw.json    # JSON5 format
```

### Minimal Config Template

```json5
{
  // Single agent, single workspace
  "agents": {
    "main": {
      "workspace": "~/openclaw-workspace",
      "model": "claude-opus-4-5",
      "provider": "anthropic"
    }
  },

  // One channel only
  "channels": {
    "telegram": {
      "enabled": true,
      "token": "${TELEGRAM_BOT_TOKEN}"
    }
  },

  // Disable non-essential features
  "cron": { "enabled": false },
  "browser": { "enabled": false },
  "multiAgent": { "enabled": false },

  // Auth
  "auth": {
    "tokenAuth": true
  },

  // State persistence
  "state": {
    "dir": "~/.openclaw/state",
    "persistent": true
  }
}
```

---

## 5. Linux/macOS Boot Paths

### Linux

```bash
# Install
curl -fsSL --proto '=https' --tlsv1.2 https://openclaw.ai/install.sh | bash

# Start gateway (foreground)
openclaw gateway --port 18789

# Start gateway (background with systemd)
sudo systemctl enable openclaw-gateway
sudo systemctl start openclaw-gateway

# Verify
curl http://localhost:18789/health
```

### macOS

```bash
# Install
curl -fsSL --proto '=https' --tlsv1.2 https://openclaw.ai/install.sh | bash

# Start gateway (foreground)
openclaw gateway --port 18789

# Start gateway (background with launchd)
# Create ~/Library/LaunchAgents/com.openclaw.gateway.plist

# Verify
curl http://localhost:18789/health
```

### Environment Variables

```bash
export ANTHROPIC_API_KEY="sk-ant-api03-xxxxx"
export TELEGRAM_BOT_TOKEN="123456:ABC-DEF..."
export OPENCLAW_STATE_DIR="~/.openclaw/state"
```

---

## 6. Feature Cut List for Mini Profile

Features explicitly **disabled** in the minimal profile:

| Feature | Config Key | Default | Mini Profile |
|---------|------------|---------|--------------|
| Multi-agent routing | `multiAgent.enabled` | true | **false** |
| Cron scheduler | `cron.enabled` | true | **false** |
| Voice channel | `channels.voice.enabled` | false | **false** |
| Live canvas | `canvas.enabled` | false | **false** |
| Browser automation | `browser.enabled` | true | **false** |
| Extra channels | `channels.*.enabled` | varies | **one only** |
| Streaming responses | `streaming` | true | **false** (match MimiClaw) |

---

## 7. Mapping to MimiClaw Concepts

| OpenClaw Concept | MimiClaw Equivalent | Notes |
|------------------|---------------------|-------|
| Gateway control plane | Task orchestration in `app_main()` | `main/mimi.c:83-146` |
| Agent loop/tool cycle | `agent/agent_loop.c` | ReAct loop, max 10 iterations |
| Workspace/state dirs | SPIFFS + NVS split | `/spiffs/` for files, NVS for config |
| Channel adapters | `telegram/` + `gateway/ws_server.c` | Telegram poller + WebSocket server |
| Provider abstraction | `llm/llm_proxy.c` | Anthropic/OpenAI dual-provider support |
| Tool registry | `tools/tool_registry.c` | JSON schema builder, dispatch by name |
| Session storage | `memory/session_mgr.c` | JSONL files per chat_id |
| Long-term memory | `memory/memory_store.c` | MEMORY.md + daily notes |
| System prompt | `agent/context_builder.c` | SOUL.md + USER.md + memory + tools |
| Config management | `mimi_config.h` + NVS | Build-time defaults + runtime CLI override |

### Architecture Correspondence

```
OpenClaw Gateway          MimiClaw ESP32-S3
─────────────────         ─────────────────
Gateway Process     ↔     app_main() orchestration
Agent Manager       ↔     agent_loop task (Core 1)
Channel Manager     ↔     telegram_bot + ws_server
Tool Executor       ↔     tool_registry_execute()
State Store         ↔     SPIFFS filesystem
Config Service      ↔     mimi_config.h + NVS
Message Queue       ↔     message_bus (FreeRTOS queues)
```

---

## 8. Migration Risks and Guardrails

### Memory Constraints

| Constraint | OpenClaw | MimiClaw | Risk |
|------------|----------|----------|------|
| Max tokens | 128K+ | 4096 | Token budget overflow |
| Session history | Unlimited | 20 messages | Context truncation |
| Concurrent requests | Many | 1 | No parallelism |
| Tool output size | Large | 8 KB | Truncation required |

### Behavioral Differences

1. **Streaming**: OpenClaw streams tokens; MimiClaw uses non-streaming JSON
2. **Tool timeout**: OpenClaw has configurable timeouts; MimiClaw uses fixed 120s HTTP timeout
3. **Error recovery**: OpenClaw has retry logic; MimiClaw fails fast
4. **Multi-turn limit**: Both default to 10 iterations, but OpenClaw is configurable

### Guardrails for Parity Testing

- Set `max_tokens: 4096` in OpenClaw config to match MimiClaw
- Limit session history to 20 messages
- Disable streaming in OpenClaw for behavior parity
- Use identical system prompts (SOUL.md, USER.md content)

---

## 9. Reference Commands

### OpenClaw CLI

```bash
# Install
curl -fsSL --proto '=https' --tlsv1.2 https://openclaw.ai/install.sh | bash

# Start gateway
openclaw gateway --port 18789

# Check status
openclaw status

# View logs
openclaw logs --follow

# Stop gateway
openclaw stop
```

### MimiClaw Serial CLI (for comparison)

```
mimi> config_show              # Show all config
mimi> set_model_provider anthropic
mimi> set_api_key sk-ant-...
mimi> set_model claude-opus-4-5
mimi> wifi_status
mimi> heap_info
mimi> restart
```

### API Endpoints

| Endpoint | OpenClaw | MimiClaw |
|----------|----------|----------|
| Health | `GET /health` | N/A (no HTTP server) |
| WebSocket | `ws://localhost:18789/ws` | `ws://<esp-ip>:18789` |
| Telegram | Via channel config | Via `telegram_bot.c` |

---

## 10. Sources

### Local Files (MimiClaw)

- `main/mimi.c` — Entry point, startup sequence
- `main/mimi_config.h` — All compile-time constants
- `main/cli/serial_cli.c` — CLI command definitions
- `main/llm/llm_proxy.c` — LLM provider abstraction
- `main/agent/agent_loop.c` — ReAct loop implementation
- `main/tools/tool_registry.c` — Tool registration and dispatch
- `main/memory/session_mgr.c` — Session storage
- `main/memory/memory_store.c` — Long-term memory
- `docs/ARCHITECTURE.md` — System design documentation

### OpenClaw References

- [OpenClaw Installer](https://github.com/openclaw/openclaw/blob/main/docs/install/installer.md)
- [Gateway Documentation](https://github.com/openclaw/openclaw/blob/main/docs/gateway/index.md)
- [Configuration Examples](https://github.com/openclaw/openclaw/blob/main/docs/gateway/configuration-examples.md)
- [Sandboxing](https://github.com/openclaw/openclaw/blob/main/docs/gateway/sandboxing.md)
- [FAQ](https://github.com/openclaw/openclaw/blob/main/docs/help/faq.md)
- [README](https://github.com/openclaw/openclaw/blob/main/README.md)
