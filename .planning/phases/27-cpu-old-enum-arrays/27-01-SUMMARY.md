---
phase: 27-cpu-old-enum-arrays
plan: 01
subsystem: cpu-collect
tags: [enum, array, perf, linux, proc-stat]

# Dependency graph
requires:
  - phase: 26-themekey-enum-arrays
    provides: Established enum-indexed array pattern for replacing string-keyed maps
provides:
  - CpuOldField enum in btop_shared.hpp with 12 entries
  - cpu_old_to_field constexpr mapping array for CpuOldField-to-CpuField translation
  - Linux cpu_old as std::array<long long, CpuOldField::COUNT> (zero-initialized)
affects: [27-02, 28-posix-io]

# Tech tracking
tech-stack:
  added: []
  patterns: [enum-indexed delta tracking arrays, constexpr cross-enum mapping arrays]

key-files:
  created: []
  modified:
    - src/btop_shared.hpp
    - src/linux/btop_collect.cpp

key-decisions:
  - "CpuOldField enum placed immediately after CpuField in shared header for locality"
  - "cpu_old_to_field mapping in header (not local to Linux file) since it bridges shared enums"
  - "Kept #include <unordered_map> in linux/btop_collect.cpp -- used by 8+ other data structures"

patterns-established:
  - "Cross-enum constexpr mapping arrays for bridging related enum indices (cpu_old_to_field)"
  - "Contiguous enum arithmetic: CpuOldField::user + ii for loop indexing over time fields"

requirements-completed: [PERF-01]

# Metrics
duration: 2min
completed: 2026-03-02
---

# Phase 27 Plan 01: CpuOldField Enum Array Summary

**CpuOldField enum with 12 entries replacing string-keyed unordered_map for Linux CPU delta calculations**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-02T18:34:57Z
- **Completed:** 2026-03-02T18:36:33Z
- **Tasks:** 1
- **Files modified:** 2

## Accomplishments
- Defined CpuOldField enum in btop_shared.hpp with totals, idles, and all 10 /proc/stat time fields
- Converted Linux cpu_old from unordered_map<string, long long> to std::array<long long, 12>
- Replaced all string-keyed .at() accesses with O(1) enum-indexed [] array access
- Removed time_names string array and linux_time_fields local array (superseded by header definitions)
- Added cpu_old_to_field constexpr mapping for CpuOldField-to-CpuField translation in collect loop

## Task Commits

Each task was committed atomically:

1. **Task 1: Define CpuOldField enum and convert Linux cpu_old to std::array** - `13cdb1d` (feat)

**Plan metadata:** (pending final commit)

## Files Created/Modified
- `src/btop_shared.hpp` - Added CpuOldField enum (12 entries) and cpu_old_to_field constexpr mapping array
- `src/linux/btop_collect.cpp` - Replaced cpu_old unordered_map with std::array, removed time_names and linux_time_fields, updated all collect() delta calculations

## Decisions Made
- Placed CpuOldField enum immediately after CpuField in shared header for logical grouping of CPU-related enums
- Put cpu_old_to_field mapping in header rather than local to Linux file since it bridges two shared enum types
- Kept #include <unordered_map> in linux/btop_collect.cpp since 8+ other data structures still use it

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- CpuOldField enum and cpu_old_to_field mapping are available in btop_shared.hpp for other platform collectors
- Plan 27-02 can proceed to migrate BSD/macOS cpu_old usage (if applicable)
- All 330 tests pass, build clean with zero warnings

## Self-Check: PASSED

- FOUND: src/btop_shared.hpp
- FOUND: src/linux/btop_collect.cpp
- FOUND: 27-01-SUMMARY.md
- FOUND: commit 13cdb1d

---
*Phase: 27-cpu-old-enum-arrays*
*Completed: 2026-03-02*
