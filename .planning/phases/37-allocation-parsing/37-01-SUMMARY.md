---
phase: 37-allocation-parsing
plan: 01
subsystem: performance
tags: [pmr, monotonic_buffer_resource, arena, memory_resource, c++17]

# Dependency graph
requires: []
provides:
  - "Per-cycle pmr::monotonic_buffer_resource arena in Runner::_runner()"
  - "Unit tests validating arena allocation, release, reuse, and fallback behavior"
affects: [37-02, future pmr container adoption]

# Tech tracking
tech-stack:
  added: ["std::pmr::monotonic_buffer_resource"]
  patterns: ["per-cycle arena with O(1) release() reset"]

key-files:
  created: ["tests/test_arena.cpp"]
  modified: ["src/btop.cpp", "tests/CMakeLists.txt"]

key-decisions:
  - "Used new_delete_resource() upstream fallback for safety during development"
  - "Used alignas(64) on 64KB static buffer for cache-line alignment"
  - "Placed arena release() after safety checks but before COLLECTING STATE marker"
  - "Arena is static to persist across cycles; only release() resets the bump pointer"

patterns-established:
  - "Per-cycle arena: static monotonic_buffer_resource with release() at cycle top"
  - "Arena tests: pure pmr validation without btop integration dependency"

requirements-completed: [MEM-01]

# Metrics
duration: 4min
completed: 2026-03-14
---

# Phase 37 Plan 01: Arena Setup Summary

**64KB pmr::monotonic_buffer_resource arena in Runner::_runner() with O(1) per-cycle release() and 4 unit tests validating allocation, reset, reuse, and heap fallback**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-14T11:27:44Z
- **Completed:** 2026-03-14T11:31:58Z
- **Tasks:** 1 (TDD: RED + GREEN)
- **Files modified:** 3

## Accomplishments
- Integrated pmr::monotonic_buffer_resource arena into Runner::_runner() loop with 64KB aligned static buffer
- Arena releases with O(1) call at top of each collect/draw cycle, before any collection work
- 4 unit tests validate arena basic allocation, release/reuse, multi-cycle stability, and heap fallback
- Full test suite passes (340/340) with zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): Arena unit tests** - `fa97c0e` (test)
2. **Task 1 (GREEN): Arena integration in btop.cpp** - `548bd69` (feat)

_Note: TDD task had two commits (test then feat)_

## Files Created/Modified
- `tests/test_arena.cpp` - 4 Google Test cases for pmr arena behavior
- `tests/CMakeLists.txt` - Added test_arena.cpp to test sources
- `src/btop.cpp` - Added `#include <memory_resource>`, static arena_storage[65536], cycle_arena with new_delete_resource() upstream, cycle_arena.release() per cycle

## Decisions Made
- Used `new_delete_resource()` upstream (not `null_memory_resource()`) for development safety -- if arena is exhausted, falls back to heap instead of crashing
- Placed `cycle_arena.release()` after thread_wait safety checks and before COLLECTING STATE marker -- ensures release happens only when actually running a cycle
- Used `alignas(64)` for cache-line alignment of the arena buffer
- Arena declared static to persist across cycles; only the bump pointer is reset via release()

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed alignas syntax for AppleClang**
- **Found during:** Task 1 (GREEN phase, arena integration)
- **Issue:** `static alignas(64) std::byte` caused AppleClang error: "alignas attribute cannot be applied to types"
- **Fix:** Changed to `alignas(64) static std::byte` (alignas before storage class specifier)
- **Files modified:** src/btop.cpp
- **Verification:** Build succeeds, all 340 tests pass
- **Committed in:** 548bd69 (Task 1 GREEN commit)

**2. [Rule 3 - Blocking] Fixed gtest linking against conda installation**
- **Found during:** Task 1 (RED phase, test build)
- **Issue:** CMake found gtest from conda (`/opt/homebrew/Caskroom/miniconda/base/lib/`) with broken @rpath dylib references
- **Fix:** Reconfigured cmake with `-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` to force building gtest from source
- **Files modified:** None (build config only)
- **Verification:** Test binary links against static libgtest.a, all tests pass

---

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking)
**Impact on plan:** Both fixes necessary for correct compilation and test execution. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviations above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Arena infrastructure is in place and tested, ready for plan 37-02 (string_view audit)
- Future plans can adopt pmr containers by passing `&cycle_arena` as allocator to cycle-local temporaries
- No blockers

---
*Phase: 37-allocation-parsing*
*Completed: 2026-03-14*
