---
phase: 12-extract-transitions
plan: 01
subsystem: core
tags: [state-machine, dispatch, variant, overload-resolution, c++20]

requires:
  - phase: 11-event-queue
    provides: AppEvent variant, EventQueue, signal handler pushing events
provides:
  - event::KeyInput struct with fixed-size buffer for keyboard/mouse input
  - state_tag namespace with 6 compile-time tag types for overload resolution
  - dispatch_state() template converting AppState enum to state_tag types
  - dispatch_event() function composing dispatch_state + std::visit
  - on_event() overloads for all Running+event combinations
  - Catch-all on_event() for unhandled (state, event) pairs
affects: [12-extract-transitions, 13-entry-exit-actions]

tech-stack:
  added: []
  patterns: [state_tag dispatch, on_event overload resolution, catch-all auto template]

key-files:
  created:
    - tests/test_transitions.cpp
  modified:
    - src/btop_events.hpp
    - src/btop.cpp
    - tests/test_events.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "dispatch_event() declared in btop_events.hpp but defined in btop.cpp where on_event overloads are visible"
  - "Catch-all on_event(auto, auto, current) handles all unmatched state+event pairs by preserving current state"
  - "Running+Quit and Running+Sleep transitions have side effects (Runner::stopping) inlined in on_event"
  - "Added process_signal_event(KeyInput) no-op for variant visitor completeness"

patterns-established:
  - "state_tag dispatch: runtime AppState -> compile-time state_tag via dispatch_state template"
  - "on_event overload resolution: specific overloads for handled transitions, catch-all for unhandled"
  - "Transition tests use dispatch_event() to verify full dispatch chain without btop.cpp globals"

requirements-completed: [TRANS-01, TRANS-02]

duration: 8min
completed: 2026-02-28
---

# Plan 12-01: Transition Dispatch Infrastructure Summary

**Typed state_tag dispatch with on_event() overloads for all Running+event transitions, KeyInput event type, and 15 transition unit tests**

## Performance

- **Duration:** 8 min
- **Started:** 2026-02-28
- **Completed:** 2026-02-28
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- event::KeyInput struct with fixed-size 32-byte buffer (max 31 chars, truncation for longer keys)
- Complete state_tag namespace with 6 compile-time tags enabling overload resolution
- dispatch_state() template converting all 6 AppState values to tags without UB
- dispatch_event() composing dispatch_state + std::visit to route (AppState, AppEvent) to on_event()
- 8 specific on_event() overloads for Running state + catch-all for all other combinations
- 38 tests passing (23 new EventType/DispatchState tests + 15 Transition tests)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add KeyInput event, state_tag types, and dispatch helpers** - `8c98e18` (feat)
2. **Task 2: Define on_event() overloads and dispatch_event() in btop.cpp** - `92d06f2` (feat)

## Files Created/Modified
- `src/btop_events.hpp` - Added KeyInput, state_tag, dispatch_state, dispatch_event declaration
- `src/btop.cpp` - Added on_event() overloads, catch-all, dispatch_event() definition
- `tests/test_events.cpp` - Updated variant count, added KeyInput and DispatchState tests
- `tests/test_transitions.cpp` - Created with 15 transition tests
- `tests/CMakeLists.txt` - Added test_transitions.cpp to btop_test sources

## Decisions Made
- dispatch_event() definition placed in btop.cpp (not header) because on_event overloads are static in btop.cpp
- Added process_signal_event(KeyInput) no-op to maintain visitor completeness for current main loop
- Catch-all uses `const auto&` parameters for maximum flexibility
- Side-effecting transitions (Quit, Sleep) inline Runner::stopping in on_event (matching existing behavior)

## Deviations from Plan

### Auto-fixed Issues

**1. Added process_signal_event(KeyInput) no-op**
- **Found during:** Task 2 (on_event overloads)
- **Issue:** Adding KeyInput to AppEvent variant requires a process_signal_event overload for the existing std::visit in the main loop
- **Fix:** Added `static void process_signal_event(const event::KeyInput&) {}` no-op
- **Files modified:** src/btop.cpp
- **Verification:** Build succeeds, all tests pass
- **Committed in:** 92d06f2 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (blocking build fix)
**Impact on plan:** Essential for correctness. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Dispatch infrastructure ready for Plan 02 to rewire main loop
- process_signal_event overloads still present (Plan 02 removes them)
- on_event overloads coexist with process_signal_event (Plan 02 makes the switch)

---
*Phase: 12-extract-transitions*
*Completed: 2026-02-28*
