---
phase: 01-profiling-baseline
plan: 02
subsystem: cli
tags: [benchmark, cli, json, performance, headless]

requires:
  - phase: none
    provides: first plan - standalone CLI modification
provides:
  - btop --benchmark N macro-benchmark mode
  - Headless collect+draw cycle execution
  - Structured JSON timing output (per-cycle + summary)
  - hyperfine-compatible startup benchmarking
affects: [01-03-ci-benchmark, optimization-phases]

tech-stack:
  added: []
  patterns: [headless benchmark mode, JSON timing output]

key-files:
  created: []
  modified:
    - src/btop_cli.hpp
    - src/btop_cli.cpp
    - src/btop.cpp

key-decisions:
  - "Benchmark mode initializes Config/Theme/Shared but skips terminal init"
  - "JSON output uses fmt::format (no JSON library dependency)"
  - "GPU_SUPPORT handled with conditional compilation for Cpu::draw signature"
  - "Term dimensions set to 200x50 for consistent headless draw output"

patterns-established:
  - "CLI flags follow existing parse pattern with try/catch validation"
  - "Benchmark mode inserts before Term::init() to avoid tty requirement"

requirements-completed: [PROF-01]

duration: 11min
completed: 2026-02-27
---

# Phase 1 Plan 02: Benchmark CLI Mode Summary

**Headless `--benchmark N` mode running N collect+draw cycles with per-cycle JSON timing breakdown (wall/collect/draw) and summary statistics**

## Performance

- **Duration:** 11 min
- **Started:** 2026-02-27T07:47:00Z
- **Completed:** 2026-02-27T07:58:16Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Added --benchmark / -b CLI flag with input validation (>= 1, numeric check)
- Implemented headless benchmark execution: initializes Config, Theme, Shared, Draw without terminal
- Collects timing for each cycle: wall time, collect time (CPU+Mem+Net+Proc), draw time
- Outputs structured JSON with per-cycle timings and summary statistics (mean, min, max)
- Works cross-platform (Linux, macOS, FreeBSD) without platform-specific flags

## Task Commits

1. **Task 1: CLI parser --benchmark flag** - `4875fc4` (feat)
2. **Task 2: Benchmark execution mode** - `3385de2` (feat)

## Files Created/Modified
- `src/btop_cli.hpp` - Added benchmark_cycles field to Cli struct
- `src/btop_cli.cpp` - --benchmark/-b parsing with validation, help text
- `src/btop.cpp` - Benchmark execution path in btop_main before Term::init

## Decisions Made
- Placed benchmark path before Term::init() so it doesn't require a terminal
- Used fmt::format for JSON construction (no additional dependencies needed)
- Set Draw::calcSizes() in benchmark mode for realistic draw measurements

## Deviations from Plan
None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- btop --benchmark N ready for CI integration (Plan 01-03)
- JSON output compatible with convert_nanobench_json.py consumption
- hyperfine can measure btop --benchmark 1 for startup timing

---
*Phase: 01-profiling-baseline*
*Completed: 2026-02-27*
