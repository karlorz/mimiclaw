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
| YYYY-MM-DD | e.g. ESP32-S3 DevKitC-1 | `<sha>` | host-baseline | n/a | `make host-ci` + key log lines | PASS/FAIL |
| YYYY-MM-DD | e.g. ESP32-S3 DevKitC-1 | `<sha>` | host-live | anthropic | `make host-ci-live-anthropic` + response snippet | PASS/FAIL |
| YYYY-MM-DD | e.g. ESP32-S3 DevKitC-1 | `<sha>` | host-live | openai | `make host-ci-live-openai` + response snippet | PASS/FAIL |
| YYYY-MM-DD | e.g. ESP32-S3 DevKitC-1 | `<sha>` | firmware-build | n/a | `make firmware-ci` + build summary | PASS/FAIL |
| YYYY-MM-DD | e.g. ESP32-S3 DevKitC-1 | `<sha>` | device-smoke | anthropic/openai | CLI transcript + tool-use transcript | PASS/FAIL |

## Release Decision Rule

Only mark a commit as "fully deployable" when:
- All gates above pass.
- Evidence rows are present for each gate.
- Host baseline + host live dual-provider + firmware/device validation reference the same mainline commit SHA.
