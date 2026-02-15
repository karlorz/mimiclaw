# OpenClaw Minimal Design for Linux/macOS

## Purpose and Scope
This document defines a minimal OpenClaw runtime profile for Linux/macOS and maps it to MimiClaw implementation touchpoints. It is intentionally operational: use it to bootstrap the smallest viable setup, then scale only when a concrete requirement exists.

## What "Minimal OpenClaw" Means
Minimal OpenClaw means:
- One gateway process as the control plane.
- One agent (`main`) with one workspace.
- One enabled ingress channel at a time.
- Token auth enabled.
- Persistent state directory enabled.
- Optional browser node disabled by default.
- No cron jobs and no multi-agent routing by default.

Core concept: the OpenClaw Gateway orchestrates channels, sessions, agents, and tools.

## Smallest Viable Runtime Components
Required:
- OpenClaw install + binary on PATH.
- Gateway process.
- One agent definition (`main`).
- One workspace directory.
- One enabled channel adapter.
- Token auth.
- Persistent state directory.

Deliberately excluded from the default mini profile:
- Multi-agent routing.
- Cron service.
- Voice/talk pipeline.
- Live canvas.
- Additional channel adapters beyond one ingress.
- Browser node (unless explicitly required for the use case).

## Minimal Config Baseline (from official examples)
Config location: `~/.openclaw/openclaw.json` (JSON5).

Pattern baseline (official examples distilled to mini profile intent):

```json5
{
  // keep one primary agent
  agents: {
    main: {
      workspace: "/ABS/PATH/workspace"
    }
  },

  // keep gateway as the only control-plane process
  gateway: {
    port: 18789,
    auth: {
      token: "REPLACE_WITH_STRONG_TOKEN"
    },
    state_dir: "/ABS/PATH/state",
    channels: {
      // constrain to one ingress channel at a time
      allow: ["websocket"]
    }
  }
}
```

Operational intent from examples:
- Agent workspace is explicit and singular.
- Channel set is constrained by allowlist.
- Gateway carries auth and persistent state configuration.

## Linux/macOS Boot Paths
Installer path:

```bash
curl -fsSL --proto '=https' --tlsv1.2 https://openclaw.ai/install.sh | bash
```

Local run path:

```bash
openclaw gateway --port 18789
```

Minimal bootstrap flow:
1. Install OpenClaw with the installer command.
2. Create/edit `~/.openclaw/openclaw.json` using the mini baseline pattern.
3. Ensure exactly one ingress channel is enabled.
4. Start gateway with `openclaw gateway --port 18789`.
5. Verify token auth is required before allowing external clients.

## Feature Cut List for Mini Profile
Default cuts for mini profile:
- Multi-agent routing.
- Cron scheduler.
- Voice/talk features.
- Live canvas.
- Extra channels (keep one ingress only).
- Optional browser node (off by default; enable only when needed).

## Mapping to MimiClaw Concepts
Compact mapping table:

| OpenClaw Concept | MimiClaw Equivalent |
|---|---|
| Gateway control plane | task orchestration in `app_main()` |
| Agent loop/tool cycle | `agent/agent_loop.c` |
| Workspace/state dirs | SPIFFS + NVS split |
| Channel adapters | `telegram/` + `gateway/ws_server.c` |
| Provider abstraction | `llm/llm_proxy.c` |

## Migration Risks and Guardrails
Risks:
- Enabling multiple channels or agents too early increases operational complexity and troubleshooting cost.
- Disabling token auth on LAN-exposed gateways creates immediate abuse risk.
- Turning on browser/cron/voice before baseline stability increases failure surface.
- Confusing embedded persistence (SPIFFS/NVS) with host filesystem semantics can break portability assumptions.

Guardrails:
- Start with one channel and one agent; only expand after explicit load/use-case evidence.
- Require token auth in every non-local testing environment.
- Keep persistent `state_dir` enabled from day one.
- Treat browser node as opt-in capability, not a default dependency.
- Keep OpenClaw mini profile defaults aligned with MimiClaw minimality goals.

## Reference Commands
OpenClaw:

```bash
# Install
curl -fsSL --proto '=https' --tlsv1.2 https://openclaw.ai/install.sh | bash

# Run gateway locally
openclaw gateway --port 18789

# Edit config baseline
$EDITOR ~/.openclaw/openclaw.json
```

MimiClaw alignment references:

```bash
rg -n "void app_main" main/mimi.c
rg -n "llm_chat_tools|MIMI_AGENT_MAX_TOOL_ITER" main/agent/agent_loop.c
rg -n "MIMI_SPIFFS|MIMI_NVS_" main/mimi_config.h
rg -n "set_model_provider|set_api_key|set_search_key|set_proxy|config_show|config_reset" main/cli/serial_cli.c
```

## Sources
OpenClaw (Context7 extraction targets):
- Context7 library: `/openclaw/openclaw`
- https://github.com/openclaw/openclaw/blob/main/docs/install/installer.md
- https://github.com/openclaw/openclaw/blob/main/docs/gateway/index.md
- https://github.com/openclaw/openclaw/blob/main/docs/help/faq.md
- https://github.com/openclaw/openclaw/blob/main/docs/gateway/configuration-examples.md
- https://github.com/openclaw/openclaw/blob/main/docs/gateway/sandboxing.md
- https://github.com/openclaw/openclaw/blob/main/README.md

MimiClaw local grounding:
- `main/mimi.c`
- `main/agent/agent_loop.c`
- `main/mimi_config.h`
- `main/cli/serial_cli.c`
- `main/llm/llm_proxy.c`
- `main/gateway/ws_server.c`
- `README.md`
