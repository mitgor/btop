---
phase: 14
slug: runner-fsm
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-01
---

# Phase 14 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Google Test (gtest) via CTest |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cmake --build build-test --target btop_test && ctest --test-dir build-test/tests --output-on-failure -R "Runner"` |
| **Full suite command** | `cmake --build build-test --target btop_test && ctest --test-dir build-test/tests --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build-test --target btop_test && ctest --test-dir build-test/tests --output-on-failure -R "Runner"`
- **After every plan wave:** Run `cmake --build build-test --target btop && cmake --build build-test --target btop_test && ctest --test-dir build-test/tests --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 14-01-01 | 01 | 1 | RUNNER-01 | unit | `ctest -R "RunnerState"` | ❌ W0 | ⬜ pending |
| 14-01-02 | 01 | 1 | RUNNER-02 | unit | `ctest -R "RunnerState"` | ❌ W0 | ⬜ pending |
| 14-02-01 | 02 | 2 | RUNNER-03 | unit+build | `cmake --build build-test --target btop` | ✅ | ⬜ pending |
| 14-02-02 | 02 | 2 | RUNNER-04 | unit+build | `cmake --build build-test --target btop` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_runner_state.cpp` — stubs for RUNNER-01, RUNNER-02 (state structs, variant, tag mapping)
- [ ] `tests/CMakeLists.txt` — add test_runner_state.cpp to btop_test sources

*Existing test infrastructure (gtest, CTest, CMake) covers framework needs.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Binary semaphore wake latency unchanged | RUNNER-04 | Timing-sensitive, requires live process observation | Run btop, observe CPU metrics update timing matches pre-phase behavior |
| Data collection produces identical metrics | RUNNER-04 | Visual output comparison | Run btop side-by-side with pre-phase binary, verify identical metrics display |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
