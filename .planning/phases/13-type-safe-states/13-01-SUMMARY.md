---
phase: 13-type-safe-states
plan: 01
subsystem: state-machine
tags: [std-variant, state-structs, enum-rename, type-safety, cpp20]

# Dependency graph
requires:
  - phase: 10-name-states
    provides: "AppState enum with atomic cross-thread semantics"
  - phase: 12-extract-transitions
    provides: "dispatch_event/on_event transition infrastructure using AppState"
provides:
  - "AppStateTag enum (renamed from AppState) for cross-thread atomic reads"
  - "state:: namespace with 6 typed structs carrying per-state data"
  - "AppStateVar std::variant type for main-thread authoritative state"
  - "to_tag() helper converting variant alternative to AppStateTag"
  - "27 new tests covering state structs, variant, and to_tag mapping"
affects: [13-02-PLAN, 14-variant-transitions, 15-runner-fsm]

# Tech tracking
tech-stack:
  added: []
  patterns: ["Dual-representation state: atomic tag for cross-thread, variant for main-thread"]

key-files:
  created: []
  modified:
    - src/btop_state.hpp
    - src/btop_events.hpp
    - src/btop.cpp
    - src/btop_draw.cpp
    - src/btop_tools.cpp
    - tests/test_app_state.cpp
    - tests/test_events.cpp
    - tests/test_transitions.cpp

key-decisions:
  - "state:: namespace chosen over nested Global:: to keep variant types at file scope for ergonomic using declarations"
  - "AppStateVar defined at file scope (not inside Global::) since it is main-thread only, distinct from the cross-thread AppStateTag"
  - "to_tag() uses if-constexpr chain inside std::visit for compile-time exhaustiveness with zero runtime overhead"

patterns-established:
  - "Dual state representation: AppStateTag (atomic, cross-thread) + AppStateVar (variant, main-thread only)"
  - "state:: structs carry per-state data (Running has timing, Quitting has exit_code, Error has message)"

requirements-completed: [STATE-02, STATE-03]

# Metrics
duration: 5min
completed: 2026-02-28
---

# Phase 13 Plan 01: Type-Safe State Foundation Summary

**AppStateTag enum rename + state:: structs with per-state data + AppStateVar variant + to_tag() helper across 8 files**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-28T14:19:09Z
- **Completed:** 2026-02-28T14:24:15Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Renamed AppState enum to AppStateTag across all 8 source and test files (mechanical rename, zero semantic changes)
- Defined 6 typed state structs in state:: namespace with per-state data (Running: update_ms/future_time, Quitting: exit_code, Error: message)
- Created AppStateVar variant type enabling compile-time exclusion of invalid state combinations
- Implemented to_tag() conversion helper for shadow-updating the atomic tag from variant state
- Added 27 new tests across 3 test suites (StateData, StateVariant, StateTag) all passing

## Task Commits

Each task was committed atomically:

1. **Task 1: Define state structs, rename AppState to AppStateTag, update all consumers** - `d0e3463` (feat)
2. **Task 2: Update and add tests for state structs, variant, and to_tag** - `1bba5fb` (test)

## Files Created/Modified
- `src/btop_state.hpp` - Added state:: structs, AppStateVar variant, to_tag() helper; renamed AppState to AppStateTag
- `src/btop_events.hpp` - Updated dispatch_state/dispatch_event signatures to use AppStateTag
- `src/btop.cpp` - Mechanical rename of all 53 AppState occurrences to AppStateTag
- `src/btop_draw.cpp` - Renamed 3 AppState references to AppStateTag
- `src/btop_tools.cpp` - Renamed 2 AppState references to AppStateTag
- `tests/test_app_state.cpp` - Renamed existing tests + added 27 new tests for structs/variant/to_tag
- `tests/test_events.cpp` - Renamed AppState references in DispatchState test
- `tests/test_transitions.cpp` - Renamed AppState references in all transition tests

## Decisions Made
- state:: namespace placed outside Global:: since variant types are main-thread-only and distinct from cross-thread AppStateTag
- to_tag() implemented as inline function with if-constexpr visitor for zero-overhead compile-time dispatch
- state_tag:: namespace and dispatch_state() left unchanged in btop_events.hpp (migrated in Plan 02)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Updated test_transitions.cpp (not listed in plan)**
- **Found during:** Task 1 (compilation)
- **Issue:** test_transitions.cpp also referenced AppState but was not listed in the plan's files_modified
- **Fix:** Applied the same mechanical AppState -> AppStateTag rename
- **Files modified:** tests/test_transitions.cpp
- **Verification:** Build and all 15 transition tests pass
- **Committed in:** d0e3463 (Task 1 commit)

**2. [Rule 3 - Blocking] Fixed build-test cmake configuration**
- **Found during:** Task 1 (verification)
- **Issue:** Pre-existing conda GTest installation had broken rpath; needed FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER to use fetched gtest
- **Fix:** Used cmake flag -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER for clean build
- **Files modified:** None (build config only)
- **Verification:** Full test suite builds and runs (196/197 pass, 1 pre-existing failure)

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both fixes were necessary to complete the task. No scope creep.

## Issues Encountered
- Pre-existing RingBuffer.PushBackOnZeroCapacity test failure (unrelated to this plan, not investigated)

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- AppStateVar variant type ready for Plan 02 to migrate dispatch_event to variant-based transitions
- state_tag:: namespace still exists in btop_events.hpp -- Plan 02 will replace it with state:: structs
- All cross-thread code already uses AppStateTag, no further changes needed there

---
*Phase: 13-type-safe-states*
*Completed: 2026-02-28*

## Self-Check: PASSED
