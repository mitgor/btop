---
phase: 20
slug: pda-types-skeleton
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-02
---

# Phase 20 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GTest v1.17.0 (via FetchContent) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build-test && ctest --test-dir tests -R "MenuPDA\|MenuFrame" --output-on-failure -j$(nproc)` |
| **Full suite command** | `cd build-test && ctest --test-dir tests --output-on-failure -j$(nproc)` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick run command (MenuPDA/MenuFrame tests)
- **After every plan wave:** Run full suite command
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 20-01-01 | 01 | 1 | PDA-01 | unit | `ctest -R MenuPDA` | ❌ W0 | ⬜ pending |
| 20-01-02 | 01 | 1 | PDA-02 | unit | `ctest -R MenuPDA` | ❌ W0 | ⬜ pending |
| 20-01-03 | 01 | 1 | PDA-03 | unit | `ctest -R PDAAction` | ❌ W0 | ⬜ pending |
| 20-02-01 | 02 | 1 | FRAME-02 | unit | `ctest -R MenuFrame` | ❌ W0 | ⬜ pending |
| 20-02-02 | 02 | 1 | FRAME-04 | unit | `ctest -R MenuFrame` | ❌ W0 | ⬜ pending |
| 20-02-03 | 02 | 1 | FRAME-05 | unit | `ctest -R MenuFrame` | ❌ W0 | ⬜ pending |
| 20-02-04 | 02 | 1 | PDA-05 | compile | `cmake --build . --target btop` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_menu_pda.cpp` — stubs for PDA-01, PDA-02, PDA-03
- [ ] `tests/test_menu_frames.cpp` — stubs for FRAME-02, FRAME-04, FRAME-05
- [ ] Update `tests/CMakeLists.txt` — add new test targets

*Existing GTest infrastructure covers framework needs.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| No behavior change to running app | Phase goal | Cannot verify absence of change automatically | Run btop, navigate all menus, confirm identical behavior |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
