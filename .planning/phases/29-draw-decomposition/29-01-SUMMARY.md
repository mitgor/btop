---
phase: 29-draw-decomposition
plan: 01
subsystem: ui
tags: [refactor, draw, proc, decomposition]

# Dependency graph
requires:
  - phase: 28-hot-path-posix-io
    provides: "POSIX I/O conversions for Cpu/Proc/Mem collect functions"
provides:
  - "Decomposed Proc::draw() with 5 named sub-functions"
  - "Top-level draw() orchestrator under 75 lines"
affects: [29-02, 30-unified-redraw]

# Tech tracking
tech-stack:
  added: []
  patterns: ["draw decomposition: extract sub-functions within namespace, pass locals as params, access namespace state directly"]

key-files:
  created: []
  modified:
    - src/btop_draw.cpp

key-decisions:
  - "Sub-functions are namespace-scoped (not in header) since they are implementation details"
  - "Parameters only for local variables computed in draw() before the extracted block; namespace-level state accessed directly"
  - "Removed unused parameters from sub-function signatures to eliminate compiler warnings"

patterns-established:
  - "draw decomposition pattern: each draw_proc_* function appends to shared out string by reference"
  - "follow logic separated from rendering to keep draw() as pure orchestrator"

requirements-completed: [DRAW-01, DRAW-03]

# Metrics
duration: 9min
completed: 2026-03-02
---

# Phase 29 Plan 01: Proc::draw() Decomposition Summary

**Decomposed 553-line Proc::draw() god function into 5 focused sub-functions with 75-line orchestrator, zero behavior change**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-02T21:51:01Z
- **Completed:** 2026-03-02T22:00:05Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Reduced Proc::draw() from 553 lines to 75-line orchestrator calling 5 named sub-functions
- Extracted draw_proc_follow_logic() for follow-process state and selection edge cases
- Extracted draw_proc_header() for box outline, title buttons, filter bar, sorting controls, detailed view chrome
- Extracted draw_proc_detailed() for per-cycle detailed process info rendering (CPU graph, status, memory)
- Extracted draw_proc_list() for per-process row iteration loop with gradient colors and graph updates
- Extracted draw_proc_footer() for blank lines, banner bar, scrollbar, location counter, dead-process cleanup, hide button
- All 330 existing tests pass, zero compiler warnings

## Task Commits

Each task was committed atomically:

1. **Task 1: Extract Proc::draw() sub-functions and reduce top-level to under 100 lines** - `4c26774` (refactor)
2. **Task 2: Verify Proc::draw() byte-identical output** - verification only, no code changes

## Files Created/Modified
- `src/btop_draw.cpp` - Decomposed Proc::draw() into draw_proc_follow_logic(), draw_proc_header(), draw_proc_detailed(), draw_proc_list(), draw_proc_footer()

## Decisions Made
- Sub-functions placed within `namespace Proc {}` but NOT declared in btop_draw.hpp (implementation details only)
- Each sub-function accesses Proc namespace-level state directly (width, height, x, y, selected, start, etc.) rather than passing as parameters
- Only local variables computed inside draw() before the extracted block are passed as parameters
- Removed unused parameters (data_same, graph_bg, totalMem from header; follow_process from list; followed_pid from footer; select_max, proc_banner_shown from header) to achieve zero compiler warnings

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed unused parameters causing compiler warnings**
- **Found during:** Task 1
- **Issue:** Initial extraction passed parameters that were not actually used within each sub-function's scope (data_same, graph_bg, totalMem, select_max, proc_banner_shown to header; follow_process to list; followed_pid to footer)
- **Fix:** Removed unused parameters from function signatures and call sites
- **Files modified:** src/btop_draw.cpp
- **Verification:** Rebuild with zero warnings
- **Committed in:** 4c26774 (part of task commit)

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Parameter cleanup was necessary for the "zero warnings" done criterion. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Proc::draw() decomposition complete, ready for 29-02 (additional draw module decompositions)
- All sub-functions are cleanly separated, making future refactoring of individual sections straightforward
- Phase 30 (unified redraw) can now target individual sub-functions rather than a monolithic draw()

---
*Phase: 29-draw-decomposition*
*Completed: 2026-03-02*
