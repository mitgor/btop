---
phase: 01-profiling-baseline
plan: 01
subsystem: benchmarking
tags: [nanobench, cmake, micro-benchmark, performance]

requires:
  - phase: none
    provides: first plan in project
provides:
  - Benchmark build infrastructure (CMake + nanobench)
  - String utility micro-benchmarks (Fx::uncolor, Tools::ulen, etc.)
  - Draw component micro-benchmarks (Meter, Graph, createBox)
  - Proc::collect micro-benchmark (Linux-only)
affects: [01-03-ci-benchmark, optimization-phases]

tech-stack:
  added: [nanobench v4.3.11]
  patterns: [FetchContent for benchmark deps, --json flag for CI output]

key-files:
  created:
    - benchmarks/CMakeLists.txt
    - benchmarks/bench_string_utils.cpp
    - benchmarks/bench_draw.cpp
    - benchmarks/bench_proc_collect.cpp
  modified:
    - CMakeLists.txt

key-decisions:
  - "Used nanobench via FetchContent (single-header, no vendoring needed)"
  - "Draw benchmarks use Config/Theme init for realistic Meter/Graph measurement"
  - "Proc benchmark is Linux-only with #ifdef __linux__ guard and stub for other platforms"
  - "BTOP_BENCHMARKS option defaults to OFF, preserving normal build behavior"

patterns-established:
  - "Benchmark executables link directly against libbtop OBJECT library"
  - "Each benchmark supports --json flag for machine-readable nanobench JSON output"

requirements-completed: [PROF-01, PROF-02]

duration: 11min
completed: 2026-02-27
---

# Phase 1 Plan 01: Benchmark Infrastructure Summary

**Nanobench micro-benchmark suite with string utility, draw component, and Proc::collect benchmarks producing stable timing data in human-readable and JSON formats**

## Performance

- **Duration:** 11 min
- **Started:** 2026-02-27T07:47:00Z
- **Completed:** 2026-02-27T07:58:16Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Created benchmark build infrastructure with nanobench v4.3.11 via CMake FetchContent
- Implemented string utility benchmarks covering all hot functions (Fx::uncolor, Tools::ulen/uresize/ljust/rjust/cjust, Mv::to)
- Implemented draw component benchmarks (Meter, Graph construction/update, createBox, fmt::format composition, Fx::uncolor on draw output)
- Created Linux-only Proc::collect benchmark with /proc parsing sub-benchmarks

## Task Commits

1. **Task 1: Benchmark infrastructure + string benchmarks** - `55bcf07` (feat)
2. **Task 2: Draw + Proc::collect benchmarks** - `15dbae8` (feat)

## Files Created/Modified
- `CMakeLists.txt` - Added BTOP_BENCHMARKS option and add_subdirectory(benchmarks)
- `benchmarks/CMakeLists.txt` - nanobench FetchContent, three benchmark executables
- `benchmarks/bench_string_utils.cpp` - 7 string utility micro-benchmarks
- `benchmarks/bench_draw.cpp` - 7 draw component micro-benchmarks (Meter, Graph, createBox, fmt, Mv, uncolor)
- `benchmarks/bench_proc_collect.cpp` - Linux /proc parsing + full Proc::collect benchmarks

## Decisions Made
- Used Config/Theme initialization in draw benchmarks to enable realistic Meter and Graph measurement (initially tried without, but Graph/Meter depend on theme gradients)
- Linked benchmark executables directly against libbtop and nanobench (INTERFACE library approach didn't work with OBJECT libraries)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] CMake INTERFACE library doesn't work with OBJECT library**
- **Found during:** Task 1 (benchmark build infrastructure)
- **Issue:** INTERFACE library libbtop_bench couldn't properly link the libbtop OBJECT library, causing undefined symbol errors
- **Fix:** Changed to direct target_link_libraries against libbtop and nanobench per executable
- **Files modified:** benchmarks/CMakeLists.txt
- **Verification:** All benchmarks compile and link successfully
- **Committed in:** 55bcf07

**2. [Rule 3 - Blocking] Draw benchmarks crash without Config/Theme initialization**
- **Found during:** Task 2 (draw benchmarks)
- **Issue:** Graph, Meter, and createBox use Theme::g() and Config::getB() internally, causing unordered_map::at exceptions
- **Fix:** Added Config::load + Theme::updateThemes + Theme::setTheme initialization; used theme gradient names ("cpu") instead of raw color strings
- **Files modified:** benchmarks/bench_draw.cpp
- **Verification:** All 7 draw benchmarks run successfully
- **Committed in:** 15dbae8

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both fixes were necessary for correctness. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Benchmark infrastructure ready for CI integration (Plan 01-03)
- All benchmarks support --json output for github-action-benchmark consumption

---
*Phase: 01-profiling-baseline*
*Completed: 2026-02-27*
