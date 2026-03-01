---
phase: 12
slug: extract-transitions
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-02-28
---

# Phase 12 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Google Test v1.17.0 (via FetchContent) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R Transition --output-on-failure` |
| **Full suite command** | `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick run command
- **After every plan wave:** Run full suite command
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 12-01-01 | 01 | 1 | TRANS-01, TRANS-02 | unit | `ctest -R Transition --output-on-failure` | Wave 0 | ⬜ pending |
| 12-02-01 | 02 | 2 | EVENT-04, TRANS-04 | unit + manual | `ctest --output-on-failure` + manual | Wave 0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_transitions.cpp` — covers TRANS-01, TRANS-02, TRANS-04 (transition function unit tests)
- [ ] Update `tests/CMakeLists.txt` — add `test_transitions.cpp` to `btop_test` sources
- [ ] Update `tests/test_events.cpp` — add KeyInput event type tests

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Main loop dispatches signal events correctly | EVENT-04 | Requires live signal interaction | Resize terminal, Ctrl+C, Ctrl+Z/fg, kill -USR2 |
| Key input works identically | EVENT-04 | Requires terminal interaction | Navigate menus, use keyboard shortcuts |
| Priority semantics preserved | TRANS-04 | Requires rapid multi-signal testing | Send signals in quick succession |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
