---
phase: 02-string-allocation-reduction
plan: 01
subsystem: performance
tags: [string-optimization, ansi-parser, const-ref, zero-alloc]

# Dependency graph
requires:
  - phase: 01-profiling-baseline
    provides: "Benchmark infrastructure for measuring string utility performance"
provides:
  - "Zero-allocation Fx::uncolor() single-pass ANSI escape stripper"
  - "Const-ref signatures for uresize/luresize/ljust/rjust/cjust eliminating 127+ copies per frame"
  - "14-case GoogleTest suite validating uncolor correctness"
affects: [02-string-allocation-reduction, benchmark-regression]

# Tech tracking
tech-stack:
  added: []
  patterns: [forward-copy-parser, const-ref-string-params]

key-files:
  created:
    - tests/test_uncolor.cpp
  modified:
    - src/btop_tools.hpp
    - src/btop_tools.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Removed <regex> include from btop_tools.hpp since no remaining regex usage in that header"
  - "Used forward-copy O(n) parser for uncolor instead of erase-in-loop O(n^2) pattern that caused musl issues"
  - "Replaced str.resize() with str.substr() returns for const-ref correctness in uresize/ljust/rjust/cjust"

patterns-established:
  - "Forward-copy ANSI parser: scan for ESC[, consume digits/semicolons, check terminator, skip or copy"
  - "Const-ref string params: functions that only read input take const string& and return new strings"

requirements-completed: [STRN-01, STRN-02]

# Metrics
duration: 6min
completed: 2026-02-27
---

# Phase 2 Plan 1: Uncolor Regex Elimination and String Param Optimization Summary

**Zero-alloc single-pass ANSI escape stripper replacing std::regex uncolor(), plus const-ref signatures for 5 string utility functions eliminating copy-on-entry at 127+ call sites**

## Performance

- **Duration:** 6 min
- **Started:** 2026-02-27T08:39:14Z
- **Completed:** 2026-02-27T08:45:24Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Replaced std::regex-based Fx::uncolor() with O(n) forward-copy ANSI SGR stripper -- eliminates regex allocation overhead on every call
- Removed dead code: escape_regex, color_regex constants and <regex> include from btop_tools.hpp
- Changed uresize, luresize, ljust, rjust, cjust from pass-by-value to const string& -- eliminates unnecessary string copies at 127+ call sites per frame
- Added 14 GoogleTest cases covering all SGR forms btop generates (reset, bold, 256-color FG/BG, truecolor FG/BG, consecutive, partial/malformed)

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace Fx::uncolor() with hand-written parser and add correctness tests** - `b2d6170` (feat)
2. **Task 2: Change string utility function parameters from by-value to const reference** - `49b6ee0` (feat)

## Files Created/Modified
- `src/btop_tools.hpp` - Removed regex constants, added uncolor declaration, changed 5 function signatures to const ref
- `src/btop_tools.cpp` - Added Fx::uncolor() single-pass implementation, updated 5 function implementations for const ref
- `tests/test_uncolor.cpp` - 14 GoogleTest cases for uncolor correctness
- `tests/CMakeLists.txt` - Added test_uncolor.cpp to test executable

## Decisions Made
- Removed `<regex>` include from btop_tools.hpp since no remaining regex usage in that header (other source files that need regex include it directly)
- Used forward-copy O(n) parser for uncolor instead of the old erase-in-loop O(n^2) pattern that was disabled due to musl compilation issues
- Replaced in-place `str.resize()` with `str.substr()` returns for const-ref correctness in uresize, ljust, rjust, cjust

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- String allocation reductions from this plan are in place
- Plan 02-02 (string_view and SSO optimizations) can proceed independently
- Benchmark infrastructure from Phase 1 available for measuring impact

## Self-Check: PASSED

All created files verified present. All commit hashes verified in git log.

---
*Phase: 02-string-allocation-reduction*
*Completed: 2026-02-27*
