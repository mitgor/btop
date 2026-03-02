---
phase: 21
slug: static-local-migration
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-02
---

# Phase 21 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GTest v1.17.0 (via FetchContent) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cmake --build build-test --target btop_test && ctest --test-dir build-test -R "MenuPDA\|MenuFrame" --output-on-failure -j$(nproc)` |
| **Full suite command** | `cmake --build build-test --target btop && cmake --build build-test --target btop_test && ctest --test-dir build-test --output-on-failure -j$(nproc)` |
| **Estimated runtime** | ~10 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick run command (build + PDA tests)
- **After every plan wave:** Run full suite command (full build + all tests)
- **Before `/gsd:verify-work`:** Full suite must be green + manual menu walkthrough
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 21-01-01 | 01 | 1 | FRAME-03 | compile+unit | `cmake --build build-test --target btop` | ✅ | ⬜ pending |
| 21-01-02 | 01 | 1 | FRAME-01 | compile+unit | `cmake --build build-test --target btop` | ✅ | ⬜ pending |
| 21-02-01 | 02 | 2 | FRAME-01 | compile+unit | `cmake --build build-test --target btop_test && ctest` | ✅ | ⬜ pending |
| 21-02-02 | 02 | 2 | FRAME-01 | compile+unit | `cmake --build build-test --target btop_test && ctest` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

*Existing infrastructure covers all phase requirements. No new test files needed — Phase 20 tests cover PDA operations, and compilation is the primary verification for static local migration.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| All menus open/navigate/close correctly | Success Criteria 4 | User-visible behavior requires visual verification | Run btop, open each menu (m/o/h), navigate with arrows, ESC to close, verify signal menus via kill |
| Resize preserves interaction state | FRAME-03 | Terminal resize is external event | Open options menu, resize terminal, verify selection is preserved but layout recalculates |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
