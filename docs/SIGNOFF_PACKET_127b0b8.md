# MimiClaw Production Readiness Sign-Off Packet

## Run Metadata

- Generated (UTC): 2026-02-16T13:41:06Z
- Candidate branch: `main`
- Candidate commit SHA: `127b0b83812420ca17a949415358275deb6686fa`
- Mainline check: SHA matches `origin/main`
- Cloud sandbox: e2b Docker `cr_igi57c92`
- Final decision: **NO-GO**

## Scenario Matrix (T1-T10)

| ID | Scenario | Status | Evidence |
|---|---|---|---|
| T1 | `make host-ci` passes keyless | FAIL | e2b run at `2026-02-16T13:37:00Z` failed with `expected session file missing: .../sessions/tg_ci_smoke.jsonl`. |
| T2 | `make host-ci-live-anthropic` passes with valid key | FAIL | Key missing in environment; failed immediately with explicit missing-key error. |
| T3 | `make host-ci-live-openai` passes with valid key | FAIL | Key missing in environment; failed immediately with explicit missing-key error. |
| T4 | `make host-ci-live-all` passes sequentially | FAIL | Stopped at anthropic step due missing key (`Error 2`). |
| T5 | Missing key fails immediately with explicit error | PASS | Both live-provider targets returned explicit provider-specific missing-key messages. |
| T6 | e2b clean run has no hidden dependency issues | PASS | `scripts/host/install-deps-ubuntu.sh` succeeded on clean sandbox and host build/tests ran. |
| T7 | `make firmware-ci` passes in ESP-IDF env | FAIL | Local run at `2026-02-16T13:38:34Z` failed: `IDF_PATH is not set`. |
| T8 | Real-device provider switching succeeds | BLOCKED (FAIL) | No ESP32-S3 hardware session available in this run. |
| T9 | Tool-use prompt reaches final response on device | BLOCKED (FAIL) | Depends on real-device run; not executed. |
| T10 | Required branch checks configured and passing before merge | FAIL | `host-live-validation.yml` not triggerable on default branch (404); candidate SHA has `state=pending`, `total_count=0` checks. |

## Required Gate Evidence

1. Host baseline gate (`make host-ci`)
- Command: `cloudrouter ssh cr_igi57c92 "cd /home/user/workspace && make host-ci"`
- Key output: `expected session file missing: /home/user/workspace/.tmp/mimiclaw-host-smoke/sessions/tg_ci_smoke.jsonl`
- Result: FAIL

2. Host live gates
- Command: `make host-ci-live-anthropic`
- Key output: `missing API key for live provider 'anthropic'`
- Result: FAIL
- Command: `make host-ci-live-openai`
- Key output: `missing API key for live provider 'openai'`
- Result: FAIL

3. Firmware build gate (`make firmware-ci`)
- Command: `make firmware-ci`
- Key output: `IDF_PATH is not set. Run this from an ESP-IDF shell or source export.sh first.`
- Result: FAIL

4. Real-device validation gate
- Not executed in this run due unavailable hardware session.
- Result: BLOCKED (FAIL)

## Additional Validation Notes

- Manual workflow check:
  - Command: `gh workflow run .github/workflows/host-live-validation.yml --ref main`
  - Output: HTTP 404 workflow not found on default branch.
- Default-branch workflow inventory:
  - `AI Code Review`
  - `Build & Release`
- Candidate SHA status:
  - Command: `gh api repos/memovai/mimiclaw/commits/127b0b83812420ca17a949415358275deb6686fa/status --jq '{state,total_count}'`
  - Output: `{"state":"pending","total_count":0}`
- Secret hygiene checks:
  - `main/mimi_secrets.h` is not tracked.
  - `main/mimi_secrets.h.example` is tracked.
  - Quick tracked-file regex scan found no obvious committed API key literals.

## Failure Artifacts

- Local copied host baseline log: `.tmp/mimiclaw-host-smoke/host.log`
- Local copied host baseline config: `.tmp/mimiclaw-host-smoke/config.json`
- Source path in e2b sandbox: `/home/user/workspace/.tmp/mimiclaw-host-smoke/host.log`

## Risk Register (with Mitigation and Rollback)

1. Baseline host smoke does not complete end-to-end session write.
- Evidence: T1 failure on missing `tg_ci_smoke.jsonl`.
- Mitigation: update baseline smoke client to ignore transient "working" status until final response, then require session file.
- Rollback: keep release blocked and ship previous known-good SHA only.

2. Live-provider validation cannot run without injected secrets.
- Evidence: T2/T3/T4 missing-key failures.
- Mitigation: define secure release checklist requiring `ANTHROPIC_API_KEY` and `OPENAI_API_KEY` in both local pre-release env and GitHub workflow secrets.
- Rollback: release as host-keyless/dev only (not production sign-off) until dual-provider live checks pass.

3. Firmware gate not runnable in current shell due missing ESP-IDF environment bootstrap.
- Evidence: T7 `IDF_PATH is not set`.
- Mitigation: standardize firmware CI runner/environment bootstrap (`source $IDF_PATH/export.sh`) before gate execution.
- Rollback: do not promote firmware artifacts from this SHA.

4. Branch-check governance appears incomplete for release gate policy.
- Evidence: missing triggerable `host-live-validation` workflow on default branch and zero checks on candidate SHA.
- Mitigation: ensure required live workflow exists on default branch and required checks are enforced before merge.
- Rollback: freeze merge/release until required checks are active and green.

## Engineering Presentation Plan (5 Slides)

1. Scope and SHA under review
- Candidate SHA, branch, date/time, definition of production-ready scope.

2. Gate matrix
- Show T1-T10 status table with PASS/FAIL/BLOCKED.

3. Evidence excerpts
- One command and one key log line per gate category (host baseline, host live, firmware, workflow/checks).

4. Risks and rollback
- Top risks from register, mitigation owner, and rollback path.

5. Recommendation
- **NO-GO** until all ten scenarios pass on the same SHA with complete evidence rows and active required checks.
