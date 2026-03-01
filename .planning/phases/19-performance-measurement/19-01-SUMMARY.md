---
phase: 19-performance-measurement
plan: 01
subsystem: testing
tags: [benchmark, performance, measurement, python, cmake]

requires:
  - phase: 18-test-doc-hygiene
    provides: clean codebase ready for final measurement
  - phase: 16-runner-error-path-purity
    provides: error paths routed through event queue
  - phase: 17-signal-transition-routing
    provides: all transitions through FSM
provides:
  - Reproducible benchmark comparison script (scripts/measure_performance.py)
  - Cumulative performance report with actual measured values (docs/PERFORMANCE.md)
  - Raw benchmark JSON data for v1.4.6 vs post-v1.2 HEAD
affects: []

tech-stack:
  added: []
  patterns: [dual-measurement benchmark methodology, os.wait4 RSS measurement]

key-files:
  created:
    - scripts/measure_performance.py
    - docs/PERFORMANCE.md
    - benchmark_results/v12_comparison_report.json
    - benchmark_results/v12_old_bench.json
    - benchmark_results/v12_new_bench.json
  modified: []

key-decisions:
  - "Used os.wait4() for RSS measurement (more reliable than parsing /usr/bin/time output)"
  - "Added /usr/bin/time -l as cross-validation (13.6% vs 13.7% agreement confirms reliability)"
  - "Reused existing v1.4.6 baseline worktree at /tmp/btop-old rather than rebuilding"

patterns-established:
  - "Dual measurement: internal --benchmark JSON + external os.wait4 RSS"
  - "270 measured cycles (6 runs x 45 warm cycles) for statistical reliability"

requirements-completed: [PERF-01, PERF-02]

duration: 8min
completed: 2026-03-01
---

# Phase 19: Performance Measurement Summary

**Cumulative v1.0+v1.1+v1.2 benchmark: -4.4% wall time (1,724->1,648us), -6.3% collect, +19.2% draw (FSM overhead), +13.7% RSS (pre-allocated buffers)**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-01
- **Completed:** 2026-03-01
- **Tasks:** 2
- **Files created:** 5

## Accomplishments
- Created self-contained measurement script that builds both binaries from source and runs 270 measured cycles per binary
- Generated PERFORMANCE.md with actual measured values, methodology documentation, and comparison with v1.0-only results
- Confirmed v1.1 architecture added ~25us draw overhead per cycle but collect gains from v1.0 largely preserved
- Both RSS measurement methods (os.wait4 and /usr/bin/time) agree within 2%, confirming measurement reliability

## Task Commits

Each task was committed atomically:

1. **Task 1: Create measurement script and run benchmarks** - `b9cd3c3` (feat)
2. **Task 2: Run benchmarks and generate PERFORMANCE.md** - `1ebf5c9` (docs)

## Files Created/Modified
- `scripts/measure_performance.py` - Self-contained benchmark comparison script (476 lines)
- `docs/PERFORMANCE.md` - Cumulative performance report with real measured values (151 lines)
- `benchmark_results/v12_comparison_report.json` - Structured comparison data
- `benchmark_results/v12_old_bench.json` - v1.4.6 baseline raw data
- `benchmark_results/v12_new_bench.json` - Current HEAD raw data

## Decisions Made
- Reused existing v1.4.6 worktree at /tmp/btop-old (already had --benchmark backport from Phase 1 work)
- Used os.wait4() as primary RSS measurement with /usr/bin/time -l as cross-validation
- Did not use powermetrics (requires sudo, not reproducible)

## Deviations from Plan
None - plan executed as written.

## Issues Encountered
- cmake test link failure (gtest dylib not found at rpath) during build-perf -- does not affect the btop binary itself, only the test executable. Not relevant to benchmarking.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 19 is the final phase of v1.2 Tech Debt milestone
- All milestones complete (v1.0, v1.1, v1.2)

## Self-Check: PASSED

- [x] scripts/measure_performance.py exists (476 lines, valid Python 3)
- [x] docs/PERFORMANCE.md exists (151 lines, contains Methodology section)
- [x] benchmark_results/v12_comparison_report.json exists with old/new/changes keys
- [x] Real measured values used (not placeholders)
- [x] Methodology section documents tool, workload, duration, environment, build flags, reproduction
- [x] Before/after comparison for both v1.4.6 and current HEAD
- [x] 2 atomic commits present

---
*Phase: 19-performance-measurement*
*Completed: 2026-03-01*
