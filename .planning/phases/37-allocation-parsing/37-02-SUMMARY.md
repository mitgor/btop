---
phase: 37-allocation-parsing
plan: 02
subsystem: performance
tags: [string_view, from_chars, read_proc_file, zero-copy, sysfs, proc]

# Dependency graph
requires:
  - phase: 37-allocation-parsing
    provides: "read_proc_file utility in btop_tools.hpp (pre-existing)"
provides:
  - "Zero-copy parsing for battery, sensor, CPU freq, and CPU stat hot paths"
  - "std::array<long long, 10> for CPU stat times (compile-time sized)"
  - "read_sysfs_int/ll/double/float helper lambda pattern for battery reads"
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns: ["read_proc_file + from_chars for numeric sysfs reads", "std::array for fixed-size /proc fields"]

key-files:
  created: []
  modified: ["src/linux/btop_collect.cpp"]

key-decisions:
  - "Used std::array<long long, 10> instead of pmr::vector for CPU stat times -- compile-time size eliminates allocator dependency"
  - "Created local helper lambdas (read_sysfs_int/ll/double/float) in update_battery() instead of file-scope functions -- keeps helpers close to usage, avoids polluting namespace"
  - "Removed try/catch blocks around battery numeric reads -- from_chars returns error code instead of throwing, cleaner control flow"

patterns-established:
  - "read_sysfs_* lambda pattern: stack buffer + read_proc_file + from_chars for zero-copy sysfs numeric reads"

requirements-completed: [MEM-03]

# Metrics
duration: 4min
completed: 2026-03-14
---

# Phase 37 Plan 02: String-View Audit Summary

**Zero-copy /proc and /sys parsing for CPU stat, CPU freq, sensor temp, and battery hot paths using read_proc_file + from_chars**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-14T11:28:19Z
- **Completed:** 2026-03-14T11:32:29Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Replaced vector<long long> times with std::array<long long, 10> -- zero heap allocation per core per CPU stat cycle
- Converted CPU frequency, sensor temperature, and battery monitoring hot-path reads from readfile() to read_proc_file() with stack buffers
- Eliminated ~20+ unnecessary std::string heap allocations per update cycle across all converted paths
- Reduced readfile() call count in btop_collect.cpp from ~30 to 12 (remaining are cold startup/discovery paths)

## Task Commits

Each task was committed atomically:

1. **Task 1: Convert CPU stat times vector to std::array and CPU freq/sensor to read_proc_file** - `d51cb1e` (feat)
2. **Task 2: Convert battery monitoring readfile calls to read_proc_file** - `00551ed` (feat)

## Files Created/Modified
- `src/linux/btop_collect.cpp` - Zero-copy parsing for CPU stat, freq, sensor temp, and battery sysfs reads

## Decisions Made
- Used std::array<long long, 10> for CPU stat times instead of pmr::vector -- the field count is always <= 10, so compile-time sizing eliminates all allocator overhead
- Created local helper lambdas in update_battery() for read_sysfs_int/ll/double/float -- keeps the pattern local and avoids namespace pollution
- Removed try/catch blocks around battery numeric reads -- from_chars returns error codes instead of throwing, producing cleaner control flow with the same safety

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All /proc and /sys hot-path parsing now uses zero-copy stack buffers
- Remaining readfile() calls are in cold startup/discovery paths (sensor enumeration, battery discovery, network address) -- not worth converting
- Phase 37 plan 03 (mimalloc evaluation) is independent and can proceed

---
*Phase: 37-allocation-parsing*
*Completed: 2026-03-14*
