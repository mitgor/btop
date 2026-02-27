---
phase: 03-io-data-collection
plan: 01
subsystem: performance
tags: [posix, proc, io, zero-alloc, string_view, from_chars, linux]

# Dependency graph
requires:
  - phase: 01-profiling-baseline
    provides: "Baseline measurements and benchmark infrastructure"
provides:
  - "read_proc_file() POSIX inline helper for zero-alloc /proc reads"
  - "Optimized readfile() without redundant fs::exists() stat"
  - "POSIX-based /proc PID parsing in Linux Proc::collect()"
affects: [03-02, 04-data-structures]

# Tech tracking
tech-stack:
  added: []
  patterns: [posix-read-with-stack-buffer, string_view-field-parsing, from_chars-numeric-parsing, rfind-comm-skip]

key-files:
  created: []
  modified:
    - src/btop_tools.hpp
    - src/btop_tools.cpp
    - src/linux/btop_collect.cpp

key-decisions:
  - "Used rfind(')') for /proc/[pid]/stat comm field skipping instead of offset-based space counting"
  - "Replaced ifstream line-by-line status parsing with string_view::find for Uid field extraction"
  - "Removed unused long_string and short_str variables from PID loop scope after conversion"

patterns-established:
  - "POSIX read pattern: read_proc_file(path, buf, sizeof(buf)) with stack char[4096] for /proc files"
  - "Path construction: snprintf prefix + memcpy suffix into char[64] path buffer"
  - "Field parsing: string_view + from_chars for zero-alloc numeric extraction from text buffers"

requirements-completed: [IODC-01, IODC-02]

# Metrics
duration: 4min
completed: 2026-02-27
---

# Phase 3 Plan 01: Proc I/O Hot Path Summary

**POSIX read_proc_file() helper replacing 5 per-PID ifstream opens with zero-alloc stack-buffer reads, plus readfile() double-stat elimination**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-27T09:28:50Z
- **Completed:** 2026-02-27T09:33:21Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Eliminated fs::exists() double-stat in readfile() -- single open+good() check replaces stat()+open()
- Created read_proc_file() POSIX inline helper using open/read/close with caller-provided stack buffer
- Replaced all 5 per-PID ifstream opens in Proc::collect() with zero-allocation POSIX reads
- Implemented robust /proc/[pid]/stat parsing using rfind(')') to handle comm names with spaces/parens
- Used string_view and std::from_chars throughout for zero-copy numeric field extraction
- Preserved cold-path ifstream usage for passwd and /proc/stat (once-per-cycle reads)

## Task Commits

Each task was committed atomically:

1. **Task 1: Fix readfile() double-stat and add read_proc_file() helper** - `0c4f110` (feat)
2. **Task 2: Replace ifstream with POSIX read in Linux Proc::collect hot path** - `33cd44d` (feat)

**Plan metadata:** (pending docs commit)

## Files Created/Modified
- `src/btop_tools.hpp` - Added read_proc_file() inline helper (Linux-only, guarded with #ifdef __linux__), added fcntl.h and unistd.h includes
- `src/btop_tools.cpp` - Removed fs::exists() guard in readfile(), replaced with file.good() check
- `src/linux/btop_collect.cpp` - Replaced 5 per-PID ifstream opens with POSIX read_proc_file() calls; rewrote /proc/[pid]/stat, status, comm, cmdline, statm parsing using stack buffers, string_view, and from_chars

## Decisions Made
- Used rfind(')') for /proc/[pid]/stat comm field skipping: eliminates the name_offset space-counting approach entirely, since rfind handles comm names with arbitrary parens/spaces
- Replaced ifstream line-by-line status parsing with string_view::find("\nUid:") for direct field extraction without getline loop overhead
- Removed now-unused long_string and short_str variables from PID loop scope (dead code after conversion)
- Kept name_offset population (rng::count of spaces in name) since it may be referenced elsewhere, even though stat parsing no longer needs it

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed dead variables after ifstream replacement**
- **Found during:** Task 2 (POSIX read replacement)
- **Issue:** long_string and short_str declared at PID loop scope were unused after converting all ifstream reads to POSIX
- **Fix:** Removed both variable declarations to prevent compiler warnings
- **Files modified:** src/linux/btop_collect.cpp
- **Verification:** Build succeeds without warnings
- **Committed in:** 33cd44d (part of Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug/dead code)
**Impact on plan:** Trivial cleanup of dead variables. No scope creep.

## Issues Encountered
None - plan executed smoothly. Build verification on macOS confirmed cross-platform compilation (Linux-specific code guarded with #ifdef __linux__).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- read_proc_file() helper available for any future /proc file reads
- Plan 03-02 (remaining I/O optimizations) can proceed
- Data structure changes (Phase 4) can build on the zero-alloc parsing patterns established here

---
*Phase: 03-io-data-collection*
*Completed: 2026-02-27*
