---
phase: 05-rendering-pipeline
plan: 01
subsystem: rendering
tags: [posix-write, stdout, terminal-output, syscall, iostream-bypass]

# Dependency graph
requires:
  - phase: 03-io-data-collection
    provides: POSIX I/O patterns established for proc file reading
provides:
  - write_stdout() POSIX helper in Tools namespace for single-syscall frame output
  - All terminal output in btop.cpp routed through write_stdout instead of iostream
affects: [05-02, 05-03, rendering-pipeline]

# Tech tracking
tech-stack:
  added: []
  patterns: [posix-write-with-eintr-retry, single-buffer-frame-output, terminal-sync-in-buffer]

key-files:
  created:
    - tests/test_write_stdout.cpp
  modified:
    - src/btop_tools.hpp
    - src/btop_tools.cpp
    - src/btop.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Moved unistd.h include out of __linux__ guard for cross-platform STDOUT_FILENO access"
  - "Used thread_local string for overlay buffer in main runner to avoid per-frame heap allocation"
  - "Used concurrent reader thread in large write test to avoid pipe buffer deadlock on macOS"

patterns-established:
  - "write_stdout pattern: build complete frame string, then single Tools::write_stdout() call"
  - "Terminal sync sequences included in write buffer, never as separate I/O operations"

requirements-completed: [REND-02]

# Metrics
duration: 7min
completed: 2026-02-27
---

# Phase 5 Plan 1: POSIX write_stdout Summary

**POSIX write() helper replacing all iostream cout output with single-syscall frame writes via write_stdout()**

## Performance

- **Duration:** 7 min
- **Started:** 2026-02-27T19:12:16Z
- **Completed:** 2026-02-27T19:19:21Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Implemented write_stdout() with POSIX write(), EINTR retry loop, and partial write handling
- Replaced all 5 terminal output sites in btop.cpp with single write_stdout() calls
- Terminal sync sequences (CSI ?2026h/l) now included in the write buffer, not as separate writes
- Added 4 unit tests verifying exact byte output, empty string safety, 100KB large writes, and char pointer overload

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement write_stdout() helper and unit tests** - `bd089db` (feat)
2. **Task 2: Replace all cout output sites with write_stdout** - `a010d0c` (feat)

## Files Created/Modified
- `src/btop_tools.hpp` - Added write_stdout() declarations in Tools namespace, moved unistd.h out of __linux__ guard
- `src/btop_tools.cpp` - Implemented write_stdout() with POSIX write() + EINTR + partial write loop, added cerrno include
- `src/btop.cpp` - Replaced 5 cout output sites with Tools::write_stdout() using pre-built string buffers
- `tests/test_write_stdout.cpp` - 4 GoogleTest tests for write_stdout correctness via pipe redirection
- `tests/CMakeLists.txt` - Added test_write_stdout.cpp to btop_test sources

## Decisions Made
- Moved `#include <unistd.h>` out of the `#ifdef __linux__` guard in btop_tools.hpp so that STDOUT_FILENO is available on macOS and FreeBSD
- Used `static thread_local string overlay_buf` in the main runner frame output site to avoid per-frame heap allocation for the overlay composition
- Used a concurrent reader thread in the large string test (100KB) to prevent pipe buffer deadlock on macOS where the default pipe buffer is 64KB

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed pipe deadlock in large write test**
- **Found during:** Task 1 (unit test execution)
- **Issue:** 100KB write to pipe deadlocked because macOS pipe buffer is 64KB; write() blocks before reader starts
- **Fix:** Added std::thread reader to drain pipe concurrently while write_stdout writes
- **Files modified:** tests/test_write_stdout.cpp
- **Verification:** All 4 WriteStdout tests pass
- **Committed in:** bd089db (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug fix in test)
**Impact on plan:** Test fix necessary for correctness on macOS. No scope creep.

## Issues Encountered
None beyond the pipe buffer test fix documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- write_stdout() foundation is in place for Phase 5 Plan 2 (string buffer pre-allocation) and Plan 3 (differential output)
- All existing tests pass (63/63), build compiles cleanly
- The cout/flush pattern is fully eliminated from btop.cpp terminal output paths

## Self-Check: PASSED

All created files verified present. Both commit hashes (bd089db, a010d0c) found in git log. SUMMARY.md created successfully.

---
*Phase: 05-rendering-pipeline*
*Completed: 2026-02-27*
