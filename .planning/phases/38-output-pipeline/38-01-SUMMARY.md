---
phase: 38-output-pipeline
plan: 01
subsystem: rendering
tags: [fmt, string-buffer, draw-pipeline, zero-alloc, output-buffer]

# Dependency graph
requires:
  - phase: 37-allocation-parsing
    provides: cycle_arena and zero-copy parsing foundations
provides:
  - "Draw functions (Cpu, Gpu, Mem, Net, Proc) accept string& out and append directly"
  - "Runner loop uses pre-reserved 256KB output buffer cleared each cycle"
  - "Zero per-draw-function string allocations in hot path"
affects: [38-02-writev-scatter-gather, future-draw-optimizations]

# Tech tracking
tech-stack:
  added: []
  patterns: [shared-output-buffer-append, void-draw-with-out-ref]

key-files:
  created:
    - tests/test_draw_buffer.cpp
  modified:
    - src/btop_shared.hpp
    - src/btop_draw.cpp
    - src/btop.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Used string& out parameter (not return value) to eliminate per-draw-function allocation"
  - "Pre-reserve 256KB in runner loop, clear each cycle -- capacity preserved across frames"
  - "Left Graph::operator() internal buffering unchanged per research recommendation"
  - "Net::draw out = box changed to out += box for correct append semantics"

patterns-established:
  - "Void draw with string& out: all draw functions append to caller-owned buffer"
  - "Runner buffer lifecycle: clear() + conditional reserve() at top of each cycle"

requirements-completed: [MEM-02]

# Metrics
duration: 6min
completed: 2026-03-14
---

# Phase 38 Plan 01: Pre-allocated Output Buffer Summary

**Draw functions append to shared 256KB pre-reserved buffer via string& out parameter, eliminating 5+ string allocations per frame**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-14T12:03:22Z
- **Completed:** 2026-03-14T12:09:48Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- All 5 draw functions (Cpu, Gpu, Mem, Net, Proc) migrated from returning string to accepting string& out
- Runner loop output buffer pre-reserved to 256KB, cleared each cycle with no reallocation
- All call sites updated: runner loop (hot path), PGO training lambdas, benchmark mode
- Full test suite passes (343 tests), benchmark mode runs successfully

## Task Commits

Each task was committed atomically:

1. **Task 1a: TDD RED - draw buffer tests** - `d25dbec` (test)
2. **Task 1b: TDD GREEN - draw function signature migration** - `69d4c7e` (feat)
3. **Task 2: Runner loop and all call sites migration** - `4061351` (feat)

_Note: TDD task split into test commit + implementation commit_

## Files Created/Modified
- `tests/test_draw_buffer.cpp` - Unit tests for buffer append pattern, capacity preservation
- `tests/CMakeLists.txt` - Added test_draw_buffer.cpp to test executable
- `src/btop_shared.hpp` - Changed 5 draw function declarations from string to void + string& out
- `src/btop_draw.cpp` - Changed 5 draw function definitions, removed local buffer allocation
- `src/btop.cpp` - Updated runner loop, PGO training, benchmark mode call sites

## Decisions Made
- Used string& out parameter (not return value) to eliminate per-draw-function allocation
- Pre-reserve 256KB in runner loop, clear each cycle -- capacity preserved across frames
- Left Graph::operator() internal buffering unchanged per research recommendation
- Net::draw `out = box` changed to `out += box` for correct append semantics with shared buffer

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Net::draw assignment vs append**
- **Found during:** Task 1 (draw function migration)
- **Issue:** Net::draw had `out = box;` which would overwrite the shared buffer instead of appending
- **Fix:** Changed to `out += box;` for correct append semantics
- **Files modified:** src/btop_draw.cpp
- **Verification:** Build succeeds, benchmark mode output correct
- **Committed in:** 69d4c7e (Task 1b commit)

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Essential fix for correctness with shared buffer pattern. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Output buffer pattern established, ready for 38-02 (writev scatter-gather I/O)
- All draw functions now append to shared buffer, enabling iovec-based frame output

---
## Self-Check: PASSED

All files exist, all commit hashes verified.

---
*Phase: 38-output-pipeline*
*Completed: 2026-03-14*
