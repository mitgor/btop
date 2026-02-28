---
phase: 11
slug: event-queue
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-02-28
---

# Phase 11 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Google Test v1.17.0 (via FetchContent) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R Event --output-on-failure` |
| **Full suite command** | `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R Event --output-on-failure`
- **After every plan wave:** Run `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 11-01-01 | 01 | 1 | EVENT-01 | unit | `ctest --test-dir build/tests -R Event --output-on-failure` | Wave 0 | ⬜ pending |
| 11-01-02 | 01 | 1 | EVENT-02 | unit | `ctest --test-dir build/tests -R EventQueue --output-on-failure` | Wave 0 | ⬜ pending |
| 11-02-01 | 02 | 2 | EVENT-03 | integration (manual) | Build succeeds + btop runs identically | manual | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_events.cpp` — stubs for EVENT-01, EVENT-02 (event types, variant properties, queue push/pop/overflow/empty/FIFO)
- [ ] Update `tests/CMakeLists.txt` — add `test_events.cpp` to `btop_test` sources

*Test infrastructure (Google Test, CMake integration) already exists — only the test file is missing.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Resize signal queues event and triggers state change | EVENT-03 | Requires terminal resize interaction | Resize terminal while btop runs, verify redraw |
| Ctrl+C quits cleanly | EVENT-03 | Requires terminal signal | Press Ctrl+C while btop runs |
| Ctrl+Z suspends, fg resumes | EVENT-03 | Requires terminal job control | Press Ctrl+Z then fg |
| SIGUSR2 reloads config | EVENT-03 | Requires external signal | `kill -USR2 <pid>` while btop runs |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
