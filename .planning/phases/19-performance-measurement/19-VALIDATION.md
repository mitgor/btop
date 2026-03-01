---
phase: 19
slug: performance-measurement
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-01
---

# Phase 19 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | ctest (CMake) + Python benchmark scripts |
| **Config file** | CMakeLists.txt, benchmarks/ |
| **Quick run command** | `cd build-test && ctest --output-on-failure -j$(sysctl -n hw.ncpu)` |
| **Full suite command** | `cd build-test && ctest --output-on-failure -j$(sysctl -n hw.ncpu)` |
| **Estimated runtime** | ~3 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build-test && ctest --output-on-failure -j$(sysctl -n hw.ncpu)`
- **After every plan wave:** Run full suite across all build configs
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 19-01-01 | 01 | 1 | PERF-01 | benchmark | `python3 benchmark_comparison.py` | ✅ | ⬜ pending |
| 19-01-02 | 01 | 1 | PERF-02 | benchmark | `python3 benchmark_comparison.py` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. The `--benchmark` CLI flag and Python comparison scripts are already built.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Methodology reproducibility | PERF-01, PERF-02 | Requires human reading documentation and following steps on a clean machine | Read PERFORMANCE.md, verify steps are complete and unambiguous |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
