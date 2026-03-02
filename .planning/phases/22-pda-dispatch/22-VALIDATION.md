---
phase: 22
slug: pda-dispatch
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-02
---

# Phase 22 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest (gtest) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `build-test/tests/btop_test --gtest_filter="MenuPDA*:PDADispatch*:InvalidateLayout*"` |
| **Full suite command** | `cmake --build build-test && build-test/tests/btop_test` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `build-test/tests/btop_test --gtest_filter="MenuPDA*:PDADispatch*:InvalidateLayout*"`
- **After every plan wave:** Run `cmake --build build-test && build-test/tests/btop_test`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 22-01-01 | 01 | 1 | PDA-04 | unit | `btop_test --gtest_filter="PDADispatch*"` | ❌ W0 | ⬜ pending |
| 22-01-02 | 01 | 1 | PDA-06 | unit | `btop_test --gtest_filter="PDADispatch*"` | ❌ W0 | ⬜ pending |
| 22-02-01 | 02 | 2 | PDA-04 | unit+build | `cmake --build build-test && btop_test` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_menu_pda.cpp` — PDADispatch test group stubs for visitor dispatch and PDAResult handling
- [ ] Existing infrastructure covers framework and fixtures

*Existing gtest infrastructure and test_menu_pda.cpp cover all phase requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Menu navigation feel | PDA-04 | Visual interaction | Open btop → Main menu → Options → ESC → verify return to Normal mode |
| Signal flow | PDA-06 | Process interaction | Select process → signal menu → send signal → verify return path |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
