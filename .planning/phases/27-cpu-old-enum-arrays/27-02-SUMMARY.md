---
phase: 27-cpu-old-enum-arrays
plan: 02
subsystem: cpu-collect
tags: [enum, array, perf, bsd, macos, freebsd, openbsd, netbsd]

# Dependency graph
requires:
  - phase: 27-cpu-old-enum-arrays
    plan: 01
    provides: CpuOldField enum in btop_shared.hpp with 12 entries
provides:
  - Enum-indexed cpu_old arrays on all 4 BSD-like platforms (macOS, FreeBSD, OpenBSD, NetBSD)
  - Zero string-keyed cpu_old.at() calls remaining in entire codebase
affects: [28-posix-io, 29-draw-decomposition]

# Tech tracking
tech-stack:
  added: []
  patterns: [enum-indexed delta tracking arrays on BSD platforms, contiguous enum arithmetic for loop indexing]

key-files:
  created: []
  modified:
    - src/osx/btop_collect.cpp
    - src/freebsd/btop_collect.cpp
    - src/openbsd/btop_collect.cpp
    - src/netbsd/btop_collect.cpp

key-decisions:
  - "Kept #include <unordered_map> in all 4 files -- each has 8+ other unordered_map users"
  - "Used contiguous enum arithmetic (CpuOldField::user + ii) matching Plan 27-01 pattern"

patterns-established:
  - "Identical enum-indexed cpu_old pattern across all 5 platforms (Linux + 4 BSD)"

requirements-completed: [PERF-02, PERF-03]

# Metrics
duration: 3min
completed: 2026-03-02
---

# Phase 27 Plan 02: BSD Platform cpu_old Enum Arrays Summary

**Enum-indexed cpu_old arrays replacing string-keyed unordered_maps on macOS, FreeBSD, OpenBSD, and NetBSD -- completing codebase-wide cpu_old migration**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-02T18:39:57Z
- **Completed:** 2026-03-02T18:43:25Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Converted cpu_old from unordered_map<string, long long> to std::array<long long, CpuOldField::COUNT> on all 4 BSD-like platforms
- Replaced all string-keyed cpu_old.at() calls with O(1) enum-indexed [] array access
- Removed time_names string arrays from all 4 platform files (no longer needed for lookups)
- Zero string-keyed cpu_old.at() calls remain anywhere in the entire src/ directory
- Zero unordered_map<string, long long> cpu_old declarations remain in the codebase

## Task Commits

Each task was committed atomically:

1. **Task 1: Convert macOS and FreeBSD cpu_old to enum-indexed arrays** - `1136184` (feat)
2. **Task 2: Convert OpenBSD and NetBSD cpu_old to enum-indexed arrays** - `61f4558` (feat)

**Plan metadata:** (pending final commit)

## Files Created/Modified
- `src/osx/btop_collect.cpp` - Replaced cpu_old unordered_map with std::array, removed time_names, updated all collect() delta calculations
- `src/freebsd/btop_collect.cpp` - Replaced cpu_old unordered_map with std::array, removed time_names, updated all collect() delta calculations
- `src/openbsd/btop_collect.cpp` - Replaced cpu_old unordered_map with std::array, removed time_names, updated all collect() delta calculations
- `src/netbsd/btop_collect.cpp` - Replaced cpu_old unordered_map with std::array, removed time_names, updated all collect() delta calculations

## Decisions Made
- Kept #include <unordered_map> in all 4 files since each has 8+ other unordered_map users beyond cpu_old
- Used contiguous enum arithmetic (CpuOldField::user + ii) for per-field loop indexing, matching the pattern established in Plan 27-01

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- cpu_old migration is 100% complete across all 5 platforms (Linux, macOS, FreeBSD, OpenBSD, NetBSD)
- Phase 27 is fully complete -- all plans executed
- Phase 28 (POSIX I/O) can proceed independently

## Self-Check: PASSED

- FOUND: src/osx/btop_collect.cpp
- FOUND: src/freebsd/btop_collect.cpp
- FOUND: src/openbsd/btop_collect.cpp
- FOUND: src/netbsd/btop_collect.cpp
- FOUND: 27-02-SUMMARY.md
- FOUND: commit 1136184
- FOUND: commit 61f4558

---
*Phase: 27-cpu-old-enum-arrays*
*Completed: 2026-03-02*
