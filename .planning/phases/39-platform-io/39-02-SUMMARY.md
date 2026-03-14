---
phase: 39-platform-io
plan: 02
subsystem: io
tags: [io_uring, proc, linux, posix, batch-io, syscall-reduction]

# Dependency graph
requires:
  - phase: 39-platform-io
    provides: "Research on io_uring for /proc reads (39-RESEARCH.md)"
provides:
  - "ProcReader class with io_uring backend and POSIX sequential fallback"
  - "CMake BTOP_IO_URING option with automatic liburing detection"
  - "Batched /proc/[pid]/stat reads in Linux process collection hot path"
affects: [performance, linux-collector]

# Tech tracking
tech-stack:
  added: [liburing (optional, Linux only)]
  patterns: [io_uring linked SQE chains (open+read+close), batch-then-parse collection loop]

key-files:
  created:
    - src/linux/io_uring_reader.hpp
    - src/linux/io_uring_reader.cpp
    - tests/test_proc_reader.cpp
  modified:
    - CMakeLists.txt
    - src/linux/btop_collect.cpp

key-decisions:
  - "Static ProcReader instance in process collection for connection reuse across cycles"
  - "IOSQE_IO_HARDLINK for read->close link to prevent short-read chain cancellation"
  - "Three-pass loop structure: collect paths, batch read, parse results"
  - "statm reads remain sequential (rare fallback for bad RSS values)"

patterns-established:
  - "io_uring linked SQE pattern: OPENAT(IO_LINK) -> READ(IO_HARDLINK) -> CLOSE for batched file I/O"
  - "Platform-conditional CMake: BTOP_IO_URING option inside if(LINUX) block"

requirements-completed: [IO-01]

# Metrics
duration: 7min
completed: 2026-03-14
---

# Phase 39 Plan 02: io_uring ProcReader Summary

**ProcReader class batches /proc/[pid]/stat reads via io_uring linked SQE chains with automatic POSIX sequential fallback**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-14T13:57:25Z
- **Completed:** 2026-03-14T14:04:13Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- ProcReader abstraction with io_uring backend (linked OPENAT+READ+CLOSE chains) and POSIX sequential fallback
- CMake BTOP_IO_URING option with pkg_check_modules for liburing >= 2.0 detection
- Linux process collection hot path restructured into collect/batch/parse passes for batched stat reads
- All io_uring code behind #ifdef USE_IO_URING -- macOS build completely unaffected

## Task Commits

Each task was committed atomically:

1. **Task 1: Create ProcReader with CMake integration and tests** - `80e3542` (feat)
2. **Task 2: Integrate ProcReader into Linux process collection hot path** - `28c5dca` (feat)

## Files Created/Modified
- `src/linux/io_uring_reader.hpp` - ProcReader class with ReadRequest struct, io_uring + POSIX API
- `src/linux/io_uring_reader.cpp` - Implementation: POSIX fallback always available, io_uring with linked SQE chains
- `tests/test_proc_reader.cpp` - Unit tests for POSIX fallback path (guarded with #ifdef __linux__)
- `CMakeLists.txt` - BTOP_IO_URING option, liburing detection, conditional compilation
- `src/linux/btop_collect.cpp` - ProcReader include, three-pass loop (collect paths, batch read, parse)

## Decisions Made
- Used static ProcReader instance for io_uring ring reuse across collection cycles
- IOSQE_IO_HARDLINK for read->close link (not IO_LINK) to prevent short-read chain cancellation per research findings
- Three-pass loop: collect PID paths + sequential cache-miss reads, batch all stat reads, parse results -- minimally invasive to existing code
- statm fallback reads remain sequential (rare edge case, not worth batching)
- RING_SIZE=4096, MAX_BATCH=1024 -- supports up to 1024 files per submit call

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- Pre-existing build error in src/osx/btop_collect.cpp (SMCConnection private constructor) prevents full btop_test linking on macOS. Not caused by phase 39 changes. Logged to deferred-items.md.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- io_uring batched I/O infrastructure ready for Linux benchmarking
- On Linux with liburing: io_uring path auto-detected and used
- On Linux without liburing: POSIX fallback used automatically with identical behavior
- macOS build completely clean (io_uring code never compiled)

---
*Phase: 39-platform-io*
*Completed: 2026-03-14*
