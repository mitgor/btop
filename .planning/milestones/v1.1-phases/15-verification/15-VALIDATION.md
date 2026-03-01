---
phase: 15
slug: verification
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-01
---

# Phase 15 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + Catch2 (existing) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build-test && ctest --test-dir tests -R "AppState\|RunnerState\|Event\|Transition" --output-on-failure -j$(nproc)` |
| **Full suite command** | `cd build-test && ctest --test-dir tests --output-on-failure -j$(nproc)` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick command (FSM-related tests)
- **After every plan wave:** Run full suite
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 15-01-01 | 01 | 1 | VERIFY-01 | unit | `ctest -R AppState` | ✅ partial | ⬜ pending |
| 15-01-02 | 01 | 1 | VERIFY-02 | unit | `ctest -R RunnerState` | ✅ partial | ⬜ pending |
| 15-02-01 | 02 | 2 | VERIFY-03 | sanitizer | `build-asan && ctest` | ❌ W0 | ⬜ pending |
| 15-02-02 | 02 | 2 | VERIFY-04 | sanitizer | `build-tsan && ctest` | ❌ W0 | ⬜ pending |
| 15-03-01 | 03 | 2 | VERIFY-05 | manual | manual smoke test | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Sanitizer CMake presets for ASan+UBSan with BUILD_TESTING=ON
- [ ] Sanitizer CMake presets for TSan with BUILD_TESTING=ON
- [ ] TSan suppression file if needed for lock-free EventQueue

*Existing test infrastructure (Catch2, CTest) covers unit test requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Visual output unchanged | VERIFY-05 | Requires terminal rendering | Run btop, verify all panels render correctly |
| Keyboard shortcuts work | VERIFY-05 | Requires interactive input | Test F2 menu, Ctrl+C quit, Ctrl+Z suspend |
| Signal handling | VERIFY-05 | Requires process signals | Send SIGWINCH, SIGUSR2, verify behavior |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
