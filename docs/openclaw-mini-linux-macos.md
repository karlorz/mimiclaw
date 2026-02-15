# OpenClaw Minimal Design for Linux/macOS

## Purpose and Scope
This document defines a minimal OpenClaw profile for Linux/macOS that preserves the OpenClaw runtime model (Node gateway) while reducing deployment and operational complexity. It is intended as a design reference for MimiClaw contributors who want OpenClaw-style behavior on desktop/server environments without adopting full platform breadth.

Scope limits:
- Keep OpenClaw runtime architecture (do not reimplement in C).
- Optimize for single-user, single-agent, single-channel operation.
- Keep only the control-plane pieces required for message-in, agent execution, tool use, and message-out.

## What “Minimal OpenClaw” Means
Minimal OpenClaw in this repo means the following canonical target profile:
- Single gateway process.
- Single agent (`main`) and one workspace.
- One enabled ingress channel at a time.
- Token auth enabled.
- Persistent state dir enabled.
- Optional browser disabled by default.
- No cron and no multi-agent by default.

This profile is intentionally conservative so it can be reasoned about like an embedded system: deterministic scope, explicit state paths, and low operational surface area.

## Smallest Viable Runtime Components
The minimal runtime still requires these components:
- Gateway control plane: receives channel events, authenticates requests, routes to sessions/agents/tools.
- Agent runtime: one primary agent identity (`main`) bound to one workspace.
- One channel adapter: exactly one ingress channel enabled at a time.
- Model provider configuration: at least one provider/API key configured.
- Persistent state/workspace paths: state directory and workspace directory on disk.

Core control-plane concept to preserve:
- OpenClaw Gateway orchestrates channels, sessions, agents, and tools.

Everything else is optional in the mini profile and starts disabled.

## Minimal Config Baseline (from official examples)
Official docs identify `~/.openclaw/openclaw.json` as the primary configuration file (JSON5).

Absolute-minimum example pattern (from OpenClaw docs):

```json5
{
  agent: { workspace: "~/.openclaw/workspace" },
  channels: { whatsapp: { allowFrom: ["+15555550123"] } },
}
```

Mini-profile baseline used in this repo (same intent, explicit gateway constraints):

```json5
{
  gateway: {
    mode: "local",
    port: 18789,
    auth: "token",
  },
  agents: {
    defaults: {
      workspace: "~/.openclaw/workspace",
    },
    list: [{ id: "main" }],
  },
  channels: {
    // Enable only one ingress channel at a time in the mini profile.
    whatsapp: { allowFrom: ["+15555550123"] },
  },
  browser: { enabled: false },
  cron: { enabled: false },
}
```

For persistence, keep state/workspace rooted on disk (for example with `OPENCLAW_STATE_DIR` and workspace path in config) so gateway restarts do not lose session data.

## Linux/macOS Boot Paths
Primary install path (macOS/Linux/WSL):

```bash
curl -fsSL --proto '=https' --tlsv1.2 https://openclaw.ai/install.sh | bash
```

Local minimal run path:

```bash
openclaw gateway --port 18789
```

Optional guided setup path:

```bash
openclaw onboard --install-daemon
```

Operational recommendation for mini profile:
- Start with local gateway mode.
- Confirm only one ingress channel is enabled.
- Confirm token auth is enabled before exposing beyond localhost/LAN.

## Feature Cut List for Mini Profile
Default cuts for this profile:
- Multi-agent routing: disabled.
- Cron service: disabled.
- Voice wake / talk mode: disabled.
- Live canvas / UI extras: disabled.
- Extra channels beyond one ingress channel: disabled.
- Browser node: disabled by default; enable only when browser automation is explicitly needed.

Keep these enabled:
- Gateway.
- One agent.
- One ingress channel.
- Tool calling for required tasks.
- Persistent state and workspace.

## Mapping to MimiClaw Concepts
| OpenClaw concept | MimiClaw equivalent |
| --- | --- |
| Gateway control plane | Task orchestration in `app_main()` |
| Agent loop/tool cycle | `agent/agent_loop.c` |
| Workspace/state dirs | SPIFFS + NVS split |
| Channel adapters | `telegram/` + `gateway/ws_server.c` |
| Provider abstraction | `llm/llm_proxy.c` |

## Migration Risks and Guardrails
Risks:
- Over-enabling channels/features too early increases failure surface and debugging noise.
- Weak auth configuration can expose a gateway that has tool execution capability.
- Non-persistent state paths can silently reset sessions/memory between restarts.
- Enabling browser/voice/canvas before baseline stability introduces difficult cross-component issues.

Guardrails:
- Keep one ingress channel until base flow is stable.
- Require token auth in all non-local test environments.
- Pin and document state/workspace paths before onboarding users.
- Add one optional subsystem at a time and validate with controlled smoke tests.

## Reference Commands
Installer:

```bash
curl -fsSL --proto '=https' --tlsv1.2 https://openclaw.ai/install.sh | bash
```

Start local gateway:

```bash
openclaw gateway --port 18789
```

Start with verbose logs:

```bash
openclaw gateway --port 18789 --verbose
```

Force replace existing listener:

```bash
openclaw gateway --force
```

Optional onboarding:

```bash
openclaw onboard --install-daemon
```

## Sources
Local files:
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/TODO.md`
- `main/mimi.c`
- `main/agent/agent_loop.c`
- `main/llm/llm_proxy.c`

Context7:
- `/openclaw/openclaw`
- `https://github.com/openclaw/openclaw/blob/main/docs/install/installer.md`
- `https://github.com/openclaw/openclaw/blob/main/docs/gateway/index.md`
- `https://github.com/openclaw/openclaw/blob/main/docs/help/faq.md`
- `https://github.com/openclaw/openclaw/blob/main/docs/gateway/configuration-examples.md`
- `https://github.com/openclaw/openclaw/blob/main/docs/gateway/sandboxing.md`
- `https://github.com/openclaw/openclaw/blob/main/README.md`
