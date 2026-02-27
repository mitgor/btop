---
phase: 01-profiling-baseline
plan: 03
subsystem: ci
tags: [github-actions, benchmark-action, ci, regression-detection]

requires:
  - phase: 01-profiling-baseline
    plan: 01
    provides: micro-benchmark executables with --json output
  - phase: 01-profiling-baseline
    plan: 02
    provides: btop --benchmark macro-benchmark mode
provides:
  - CI benchmark workflow for automated regression detection
  - JSON converter for nanobench-to-benchmark-action format
  - Linux primary baseline with auto-push to gh-pages
  - macOS secondary benchmark tracking
affects: [optimization-phases]

tech-stack:
  added: [benchmark-action/github-action-benchmark@v1, actions/checkout@v6]
  patterns: [customSmallerIsBetter format, 200% alert threshold]

key-files:
  created:
    - .github/workflows/benchmark.yml
    - benchmarks/convert_nanobench_json.py
  modified: []

key-decisions:
  - "Linux (ubuntu-24.04) is primary baseline with auto-push to gh-pages"
  - "macOS results stored separately without auto-push"
  - "200% alert threshold with comment-on-alert but no fail-on-alert"
  - "Converter handles both nanobench flat results[] and btop benchmark JSON"
  - "Proc benchmark allowed to fail (|| true) since it may not work in CI containers"

patterns-established:
  - "Benchmark results stored at dev/bench (Linux) and dev/bench-macos (macOS)"
  - "Concurrency groups per-workflow per-platform prevent duplicate runs"

requirements-completed: [PROF-03]

duration: 8min
completed: 2026-02-27
---

# Phase 1 Plan 03: CI Benchmark Workflow Summary

**GitHub Actions CI workflow with benchmark-action for automated performance regression detection across Linux and macOS**

## Performance

- **Duration:** 8 min
- **Started:** 2026-02-27T08:15:00Z
- **Completed:** 2026-02-27T08:23:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Created Python converter script to transform nanobench and btop benchmark JSON to github-action-benchmark customSmallerIsBetter format
- Created GitHub Actions workflow with Linux (primary, auto-push) and macOS (secondary) benchmark jobs
- Configured 200% alert threshold with comment-on-alert for regression notification

## Task Commits

1. **Task 1: JSON converter script** - `cdfbe5a` (feat)
2. **Task 2: CI benchmark workflow** - `51e0ce2` (feat)

## Files Created/Modified
- `benchmarks/convert_nanobench_json.py` - Converts nanobench results[] and btop benchmark.summary to customSmallerIsBetter format
- `.github/workflows/benchmark.yml` - Two-job CI workflow (benchmark-linux, benchmark-macos) with concurrency groups

## Decisions Made
- Used customSmallerIsBetter format (ns/op for micro, us/cycle for macro) - enables unified dashboard
- macOS uses brew-installed LLVM/LLD matching existing cmake-macos.yml CI pattern
- Proc benchmark runs with `|| true` in Linux job since /proc may have limited access in CI containers

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] nanobench JSON format mismatch**
- **Found during:** Task 1 (converter script)
- **Issue:** Initial converter assumed nested `results[].benchmarks[]` but nanobench uses flat `results[]` where each entry is a benchmark directly
- **Fix:** Changed to iterate `results[]` directly, accessing `name` and `median(elapsed)` per entry
- **Files modified:** benchmarks/convert_nanobench_json.py
- **Verification:** Tested with real nanobench output and btop --benchmark output
- **Committed in:** cdfbe5a

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Format fix was necessary for correctness. No scope creep.

## Issues Encountered
None

## User Setup Required
None - workflow runs automatically on push/PR to main.

## Next Phase Readiness
- All PROF requirements (PROF-01, PROF-02, PROF-03) complete
- Baseline benchmarks established for optimization phases

---
*Phase: 01-profiling-baseline*
*Completed: 2026-02-27*
