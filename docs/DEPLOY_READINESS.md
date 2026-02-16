# MimiClaw Deploy Readiness

This checklist defines the minimum evidence required before declaring MimiClaw "fully deployable".

## Required Gates

1. Host baseline gate (deterministic, keyless)

```bash
make host-ci
```

Pass criteria:
- Build succeeds.
- Host regression tests succeed.
- Keyless WebSocket smoke/robustness checks succeed.

2. Host live-provider gate (both providers required)

```bash
export ANTHROPIC_API_KEY=...
export OPENAI_API_KEY=...
make host-ci-live-anthropic
make host-ci-live-openai
# optional convenience target:
make host-ci-live-all
```

Pass criteria per provider:
- Outbound frame type is `response`.
- `chat_id` matches the request.
- Response `content` is non-empty.
- Response is not fallback text (`Sorry, I encountered an error.`).

3. Firmware build gate

```bash
make firmware-ci
```

Pass criteria:
- `idf.py set-target esp32s3` succeeds.
- `idf.py build` succeeds in ESP-IDF environment.

4. Real-device validation gate (ESP32-S3 hardware)

Required checks:
- Flash and monitor on real ESP32-S3 board.
- Provider switch through serial CLI:
  - `set_model_provider anthropic`
  - `set_model_provider openai`
- At least one successful assistant response for each provider.
- At least one prompt that triggers tool use followed by a final assistant response.

## Evidence Capture (Mandatory)

Record evidence for each gate in the table below.

| Date (UTC) | Board | Commit SHA | Gate | Provider | Command / Log Snippet | Result |
|---|---|---|---|---|---|---|
| 2026-02-16T13:37:00Z | e2b Docker sandbox `cr_igi57c92` | `127b0b83812420ca17a949415358275deb6686fa` | host-baseline | n/a | `make host-ci` -> `expected session file missing: .../sessions/tg_ci_smoke.jsonl` | FAIL |
| 2026-02-16T13:37:48Z | e2b Docker sandbox `cr_igi57c92` | `127b0b83812420ca17a949415358275deb6686fa` | host-live | anthropic | `make host-ci-live-anthropic` -> `missing API key for live provider 'anthropic'` | FAIL |
| 2026-02-16T13:37:48Z | e2b Docker sandbox `cr_igi57c92` | `127b0b83812420ca17a949415358275deb6686fa` | host-live | openai | `make host-ci-live-openai` -> `missing API key for live provider 'openai'` | FAIL |
| 2026-02-16T13:38:34Z | local shell (no ESP-IDF env) | `127b0b83812420ca17a949415358275deb6686fa` | firmware-build | n/a | `make firmware-ci` -> `IDF_PATH is not set` | FAIL |
| 2026-02-16T13:41:06Z | real ESP32-S3 not connected | `127b0b83812420ca17a949415358275deb6686fa` | device-smoke | anthropic/openai | Device validation not executed in this run (no board attached) | BLOCKED (FAIL) |

## Additional Gate Observations (2026-02-16 UTC)

- Candidate SHA is on `origin/main`: `127b0b83812420ca17a949415358275deb6686fa`.
- Manual workflow trigger check failed:
  - `gh workflow run .github/workflows/host-live-validation.yml --ref main`
  - Result: HTTP 404 workflow not found on default branch.
- Existing workflows on default branch:
  - `AI Code Review`
  - `Build & Release`
- Commit status for candidate SHA:
  - `gh api repos/memovai/mimiclaw/commits/127b0b83812420ca17a949415358275deb6686fa/status`
  - Result: `state=pending`, `total_count=0` (no status checks recorded).
- Secret hygiene spot checks:
  - `main/mimi_secrets.h` is not tracked and not present in working tree.
  - `main/mimi_secrets.h.example` is tracked.
  - No obvious API key literals found by quick tracked-file regex scan.

## Release Decision Rule

Only mark a commit as "fully deployable" when:
- All gates above pass.
- Evidence rows are present for each gate.
- Host baseline + host live dual-provider + firmware/device validation reference the same mainline commit SHA.
