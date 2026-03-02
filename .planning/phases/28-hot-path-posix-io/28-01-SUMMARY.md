---
phase: 28-hot-path-posix-io
plan: 01
subsystem: performance
tags: [posix-io, proc-stat, string-view, zero-allocation, linux]

# Dependency graph
requires:
  - phase: 27-cpu-old-enum-arrays
    provides: "CpuOldField enum and cpu_old_to_field constexpr mapping"
provides:
  - "Zero-allocation /proc/stat reads in Cpu::collect and Proc::collect via read_proc_file()"
  - "string_view-based line parsing replacing ifstream extraction operators"
affects: [28-02, performance-benchmarks]

# Tech tracking
tech-stack:
  added: []
  patterns: ["POSIX read() with stack buffers for /proc pseudo-files", "string_view line-by-line parsing with lambda integer parser"]

key-files:
  created: []
  modified: ["src/linux/btop_collect.cpp"]

key-decisions:
  - "32KB stack buffer for Cpu::collect /proc/stat (handles 256+ core systems)"
  - "1KB stack buffer for Proc::collect (only needs first aggregate line)"
  - "Inline lambda parse_ll for integer parsing instead of stream extraction"

patterns-established:
  - "string_view line iteration: find newline, substr, parse fields -- no heap allocation"
  - "POSIX read_proc_file + stack buffer pattern for hot-path /proc reads"

requirements-completed: [PERF-04]

# Metrics
duration: 4min
completed: 2026-03-02
---

# Phase 28 Plan 01: /proc/stat POSIX I/O Summary

**Zero-allocation /proc/stat reads in Cpu::collect (32KB stack buf) and Proc::collect (1KB stack buf) via read_proc_file() with string_view parsing**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-02T20:46:48Z
- **Completed:** 2026-03-02T20:50:18Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Eliminated ifstream construction and std::string heap allocations from Cpu::collect /proc/stat read path (hottest I/O path, called every update cycle)
- Eliminated ifstream construction from Proc::collect /proc/stat cputimes accumulation
- Preserved all existing logic: dynamic core detection, CpuOldField enum indexing, missing core zero-fill, core_old_totals/idles arrays, coreNum_reset notification

## Task Commits

Each task was committed atomically:

1. **Task 1: Convert Cpu::collect /proc/stat read from ifstream to read_proc_file** - `8f9af53` (feat)
2. **Task 2: Convert Proc::collect /proc/stat read from ifstream to read_proc_file** - `9cdc24f` (feat)

## Files Created/Modified
- `src/linux/btop_collect.cpp` - Replaced ifstream-based /proc/stat reading with POSIX read_proc_file() + string_view parsing in both Cpu::collect and Proc::collect

## Decisions Made
- Used 32KB stack buffer for Cpu::collect because /proc/stat has ~80 bytes per core line; 256-core systems need ~20KB so 32KB provides headroom
- Used 1KB stack buffer for Proc::collect since only the first aggregate "cpu" line (~80 bytes) is needed
- Implemented parse_ll as a local lambda within the try block for clean scoping
- Kept pread ifstream variable in Proc::collect for /etc/passwd reading (out of scope for this plan)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None - build succeeded cleanly on both tasks.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- /proc/stat reads fully converted to zero-allocation POSIX I/O
- Ready for 28-02 (per-process /proc reads) which extends the same pattern to per-PID file reads
- read_proc_file() utility proven in production use across both Cpu and Proc collectors

## Self-Check: PASSED

- FOUND: src/linux/btop_collect.cpp
- FOUND: commit 8f9af53 (Task 1)
- FOUND: commit 9cdc24f (Task 2)
- FOUND: 28-01-SUMMARY.md

---
*Phase: 28-hot-path-posix-io*
*Completed: 2026-03-02*
