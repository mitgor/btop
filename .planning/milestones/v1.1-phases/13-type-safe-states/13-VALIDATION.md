---
phase: 13
slug: type-safe-states
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-02-28
---

# Phase 13 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Google Test v1.17.0 (via FetchContent) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R "State\|Transition" --output-on-failure` |
| **Full suite command** | `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R "State\|Transition" --output-on-failure`
- **After every plan wave:** Run `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 13-01-01 | 01 | 0 | STATE-02 | unit | `ctest --test-dir build/tests -R StateData --output-on-failure` | ❌ W0 | ⬜ pending |
| 13-01-02 | 01 | 0 | STATE-03 | unit | `ctest --test-dir build/tests -R StateVariant --output-on-failure` | ❌ W0 | ⬜ pending |
| 13-01-03 | 01 | 0 | STATE-03 | unit | `ctest --test-dir build/tests -R StateTag --output-on-failure` | ❌ W0 | ⬜ pending |
| 13-01-04 | 01 | 0 | TRANS-03 | unit | `ctest --test-dir build/tests -R TypedTransition --output-on-failure` | ❌ W0 | ⬜ pending |
| 13-01-05 | 01 | 0 | TRANS-03 | unit | `ctest --test-dir build/tests -R TypedDispatch --output-on-failure` | ❌ W0 | ⬜ pending |
| 13-01-06 | 01 | 0 | TRANS-03 | unit | `ctest --test-dir build/tests -R EntryExit --output-on-failure` | ❌ W0 | ⬜ pending |
| 13-XX-XX | XX | X | ALL | integration | Build + btop runs | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_app_state.cpp` — UPDATE: AppState -> AppStateTag rename, add state struct data tests
- [ ] `tests/test_transitions.cpp` — UPDATE: enum returns -> variant returns, test state data in transitions
- [ ] `tests/test_events.cpp` — UPDATE: remove state_tag:: tests, update dispatch_event for two-variant visit
- [ ] New test functions for: to_tag() mapping, variant single-alternative guarantee, entry/exit action firing

*Important: Entry/exit action tests cannot call real clean_quit(), Draw::calcSizes(), etc. Tests verify overload resolution and routing, not side effects.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Full app runs identically after Phase 13 | ALL | Requires full runtime with terminal, threading, proc access | Build + run btop: resize terminal, Ctrl+C, Ctrl+Z/fg, kill -USR2, key input, menu navigation |
| Guard-dependent exit actions (Runner::active) | TRANS-03 | Requires mock injection or real Runner | Verified manually and in Phase 15 |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
