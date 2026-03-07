---
phase: 31-dirty-flags-foundation
plan: 01
subsystem: infra
tags: [atomic, bitflags, c++23, gtest, lock-free]

# Dependency graph
requires: []
provides:
  - DirtyBit enum with per-box bits (Cpu, Mem, Net, Proc, Gpu) and ForceFullEmit
  - PendingDirty atomic accumulator with mark()/take()/peek() API
  - 6 unit tests covering bit operations and concurrent access
affects: [32-dirty-flags-wiring, 33-dirty-flags-draw, 34-dirty-flags-integration]

# Tech tracking
tech-stack:
  added: []
  patterns: [atomic-bitmask-accumulator, fetch_or-release/exchange-acquire]

key-files:
  created: [src/btop_dirty.hpp, tests/test_dirty.cpp]
  modified: [tests/CMakeLists.txt]

key-decisions:
  - "Header-only implementation with no project dependencies for maximum portability"
  - "release/acquire memory ordering per STATE.md locked decision"

patterns-established:
  - "Atomic bitmask accumulator: fetch_or to mark, exchange(0) to take"
  - "Constexpr bitwise operator overloads for enum class types"

requirements-completed: [FLAG-01, FLAG-02, FLAG-03, FLAG-04]

# Metrics
duration: 2min
completed: 2026-03-07
---

# Phase 31 Plan 01: DirtyBit Enum and PendingDirty Summary

**Header-only DirtyBit enum (6 bits) and PendingDirty atomic accumulator with fetch_or/exchange, verified by 6 GTest cases including concurrent mark+take**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-07T19:54:29Z
- **Completed:** 2026-03-07T19:55:51Z
- **Tasks:** 1 (TDD: 2 commits)
- **Files modified:** 3

## Accomplishments
- DirtyBit enum class with uint32_t backing: Cpu, Mem, Net, Proc, Gpu, ForceFullEmit
- PendingDirty struct with lock-free mark()/take()/peek() using release/acquire ordering
- 6 unit tests all passing, full test suite green (336/336)
- Self-contained header with zero project dependencies

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): Add failing DirtyBit tests** - `77cb59b` (test)
2. **Task 1 (GREEN): Implement btop_dirty.hpp** - `a0ad7fc` (feat)

## Files Created/Modified
- `src/btop_dirty.hpp` - DirtyBit enum and PendingDirty atomic accumulator (header-only)
- `tests/test_dirty.cpp` - 6 GTest cases: single bit, multi bit, take clears, DirtyAll, ForceFullEmit independence, concurrent mark+take
- `tests/CMakeLists.txt` - Added test_dirty.cpp to btop_test executable

## Decisions Made
- Followed plan exactly: header-only, self-contained, release/acquire ordering
- No refactor commit needed -- implementation was minimal and clean from the start

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- btop_dirty.hpp ready for Phase 32 to create global PendingDirty instance
- Type is self-contained so it can be included anywhere without dependency concerns
- Plan 02 (cleanup: Proc::resized removal, redraw rename) can proceed independently

---
*Phase: 31-dirty-flags-foundation*
*Completed: 2026-03-07*
