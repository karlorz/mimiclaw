# OpenClaw Targeted Mapping Note (Reference Only)

This note captures design inputs from a shallow reference clone at:

- `/tmp/openclaw-ref-long-1771168113`

No code was generated from OpenClaw; MimiClaw protocol/API ownership remains in this repository.

## Targeted Modules Reviewed

1. Gateway config and runtime contracts
- `src/config/types.gateway.ts`
- `src/config/paths.ts`
- `src/gateway/server/ws-connection.ts`
- `src/gateway/server-ws-runtime.ts`

2. Agent runtime/tool-cycle behavior
- `src/agents/tools/agent-step.ts`
- `src/gateway/call.ts`

3. Channel adapter conventions
- `src/channels/plugins/types.adapters.ts`
- `src/channels/registry.ts`

4. Local state/session layout
- `src/config/paths.ts`
- `src/config/sessions/paths.ts`
- `src/memory/session-files.ts`

## MimiClaw Design Translation (Phase 1)

1. Gateway surface
- OpenClaw runs a multiplexed control-plane gateway (WS + HTTP methods/events).
- MimiClaw phase 1 host runtime keeps a simpler WebSocket-only ingress with fixed JSON message shape:
  - inbound: `{"type":"message","content":"...","chat_id":"..."}`
  - outbound: `{"type":"response","content":"...","chat_id":"..."}`

2. Agent execution loop
- OpenClaw uses request/response orchestration (`agent` + `agent.wait`) and session history reads.
- MimiClaw keeps its existing local ReAct loop in `main/agent/agent_loop.c`, preserving tool-iteration limits and provider semantics.

3. Channel adapter boundaries
- OpenClaw channel plugins use typed adapter contracts for setup/outbound/security/runtime status.
- MimiClaw host split mirrors this boundary by isolating channel transport in `host/platform/ws_server_host.c`, while shared core logic stays in `main/`.

4. State path discipline
- OpenClaw centralizes mutable runtime under a state root (`~/.openclaw` / env overrides).
- MimiClaw host now centralizes state under `~/.mimiclaw` (or `MIMI_STATE_ROOT`) and maps virtual `/spiffs/...` paths via host path translation.

5. Config precedence model
- OpenClaw supports layered config/env/overrides.
- MimiClaw host follows the required precedence for phase 1:
  - base: `~/.mimiclaw/config.json`
  - overrides: environment variables
  - highest for launch-time networking/paths: CLI flags (`--config`, `--ws-bind`, `--ws-port`, `--state-root`)
