---
phase: 38-output-pipeline
plan: 02
subsystem: rendering
tags: [writev, scatter-gather, iovec, zero-alloc, output-pipeline, posix-io]

# Dependency graph
requires:
  - phase: 38-output-pipeline
    provides: Pre-allocated output buffer with string& out draw pattern
provides:
  - "writev() scatter-gather output via write_stdout_iov function"
  - "Runner loop frame output uses iovec[3] instead of string concatenation"
  - "Overlay and clock paths use writev scatter-gather"
  - "frame_buf temporary string allocation eliminated"
affects: [future-terminal-output-optimizations]

# Tech tracking
tech-stack:
  added: [sys/uio.h, writev]
  patterns: [iovec-scatter-gather, write-synced-lambda]

key-files:
  created: []
  modified:
    - src/btop_tools.hpp
    - src/btop_tools.cpp
    - src/btop.cpp
    - tests/test_write_stdout.cpp

key-decisions:
  - "Used iovec[3] inline for hot-path runner frame output for clarity"
  - "Used write_synced lambda for overlay and clock paths to reduce repetition"
  - "Partial write handling advances iovec array rather than falling back to write()"

patterns-established:
  - "Scatter-gather output: build iovec array, call write_stdout_iov instead of string concat + write"
  - "write_synced lambda: reusable pattern for sync_start + content + sync_end writev"

requirements-completed: [IO-03]

# Metrics
duration: 4min
completed: 2026-03-14
---

# Phase 38 Plan 02: writev() Scatter-Gather I/O Summary

**writev() scatter-gather output eliminates frame_buf string allocation, writing sync_start + diff + sync_end in a single syscall via iovec[3]**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-14T12:12:54Z
- **Completed:** 2026-03-14T12:17:00Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Implemented write_stdout_iov with writev() syscall, EINTR retry, and proper partial-write iovec advancement
- Replaced all 3 output paths (frame, overlay, clock) with iovec scatter-gather
- Eliminated frame_buf temporary string allocation entirely from hot path
- 4 new WritevStdout tests pass; full suite 347/347 tests pass; benchmark mode runs successfully

## Task Commits

Each task was committed atomically:

1. **Task 1a: TDD RED - writev scatter-gather tests** - `a352111` (test)
2. **Task 1b: TDD GREEN - write_stdout_iov implementation** - `dd4c82b` (feat)
3. **Task 2: Replace frame/overlay/clock with writev** - `2c0993d` (feat)

_Note: TDD task split into test commit + implementation commit_

## Files Created/Modified
- `src/btop_tools.hpp` - Added write_stdout_iov declaration, sys/uio.h include
- `src/btop_tools.cpp` - Added write_stdout_iov implementation with writev() and partial write handling
- `src/btop.cpp` - Replaced 3 string-concat output paths with iovec + write_stdout_iov
- `tests/test_write_stdout.cpp` - Added 4 WritevStdout tests (single, multi, empty, large)

## Decisions Made
- Used iovec[3] inline for hot-path runner frame output for clarity
- Used write_synced lambda for overlay and clock paths to reduce repetition
- Partial write handling advances iovec array rather than falling back to write()

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Output pipeline complete (38-01 buffer + 38-02 writev)
- Phase 38 fully complete, ready for Phase 39

---
## Self-Check: PASSED

All files exist, all commit hashes verified.

---
*Phase: 38-output-pipeline*
*Completed: 2026-03-14*
