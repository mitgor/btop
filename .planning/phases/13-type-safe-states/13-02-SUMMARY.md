---
phase: 13-type-safe-states
plan: 02
subsystem: state-machine
tags: [std-variant, std-visit, entry-exit-actions, transition-function, two-variant-dispatch, cpp20]

# Dependency graph
requires:
  - phase: 13-type-safe-states (plan 01)
    provides: "AppStateTag, state:: structs, AppStateVar variant, to_tag() helper"
  - phase: 12-extract-transitions
    provides: "dispatch_event/on_event transition infrastructure, event queue"
provides:
  - "Variant-returning on_event() overloads (state::X -> AppStateVar)"
  - "Two-variant dispatch_event() using std::visit over (AppStateVar, AppEvent)"
  - "on_enter()/on_exit() entry/exit action overloads for state transitions"
  - "transition_to() function with exit/entry dispatch and shadow atomic enum update"
  - "Variant-based main loop with state::Running timing data"
  - "29 new tests (TypedTransition, TypedDispatch, EntryExit) all passing"
affects: [14-variant-transitions, 15-runner-fsm]

# Tech tracking
tech-stack:
  added: []
  patterns: ["Two-variant std::visit for state+event dispatch", "Entry/exit actions via overload sets + transition_to()", "TransitionCtx for dependency injection to entry actions"]

key-files:
  created: []
  modified:
    - src/btop.cpp
    - src/btop_events.hpp
    - tests/test_transitions.cpp
    - tests/test_events.cpp

key-decisions:
  - "Entry/exit actions placed after Runner namespace for forward declaration ordering"
  - "TransitionCtx struct used for dependency injection (cli reference) into on_enter overloads"
  - "on_event overloads are pure functions returning AppStateVar -- side effects moved to on_enter/on_exit"
  - "State chains (Reloading->Resizing->Running) handled as sequential transition_to() calls in main loop"

patterns-established:
  - "Entry/exit pattern: on_enter(state_type, ctx) / on_exit(old_type, new_type) with auto catch-all"
  - "transition_to() as single point for state changes: exit -> shadow update -> move -> entry"
  - "Two-variant visit: std::visit([](const auto& s, const auto& e) -> AppStateVar { ... }, state, event)"

requirements-completed: [STATE-02, STATE-03, TRANS-03]

# Metrics
duration: 5min
completed: 2026-02-28
---

# Phase 13 Plan 02: Variant-Based Dispatch with Entry/Exit Actions Summary

**Two-variant dispatch_event over (AppStateVar, AppEvent) with on_enter/on_exit transition actions replacing process_accumulated_state and state_tag infrastructure**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-28T14:28:02Z
- **Completed:** 2026-02-28T14:33:42Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Replaced state_tag:: dispatch with two-variant std::visit over (AppStateVar, AppEvent) for compile-time exhaustive transition routing
- Implemented entry/exit actions (on_enter for Resizing/Reloading/Sleeping/Quitting/Error, on_exit for Running->Quitting and Running->Sleeping)
- Created transition_to() function that fires exit actions, updates shadow atomic enum, and fires entry actions in correct order
- Rewrote main loop to use AppStateVar as authoritative state with state::Running carrying timing data
- Removed all legacy infrastructure: state_tag:: namespace, dispatch_state() template, process_accumulated_state()
- Added 29 new tests across 3 suites (TypedTransition, TypedDispatch, EntryExit) all passing

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate on_event to variant returns, add entry/exit actions, replace process_accumulated_state, update main loop** - `ee421a3` (feat)
2. **Task 2: Update transition and event tests for variant-based dispatch** - `ecec10b` (test)

## Files Created/Modified
- `src/btop.cpp` - on_event with state:: refs returning AppStateVar, on_enter/on_exit overloads, transition_to(), variant-based main loop
- `src/btop_events.hpp` - Removed state_tag:: namespace and dispatch_state(), updated dispatch_event signature to AppStateVar
- `tests/test_transitions.cpp` - Full rewrite with TypedTransition (19 tests), TypedDispatch (2 tests), EntryExit (8 tests)
- `tests/test_events.cpp` - Removed DispatchState test (state_tag gone), all other tests unchanged

## Decisions Made
- Entry/exit action overloads placed after Runner namespace definition to avoid forward declaration issues (on_enter(Resizing) needs Runner::screen_buffer, on_enter(Reloading) needs init_config)
- TransitionCtx struct wraps Cli::Cli reference for clean dependency injection into on_enter overloads
- on_event overloads kept as pure value-returning functions (no side effects) -- all side effects moved to on_enter/on_exit
- State chains (Reloading->Resizing->Running) handled as sequential transition_to() calls in main loop rather than nested transitions

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Reordered on_exit/on_enter/transition_to to after Runner namespace**
- **Found during:** Task 1 (build verification)
- **Issue:** on_enter(Resizing) references Runner::screen_buffer and on_enter(Reloading) calls init_config, both defined after the original placement at line 295
- **Fix:** Moved on_exit/on_enter/TransitionCtx/transition_to block to after the Runner namespace ends (line ~937), keeping on_event/dispatch_event in their original position since they are pure functions
- **Files modified:** src/btop.cpp
- **Verification:** Build succeeds with zero errors
- **Committed in:** ee421a3 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Necessary reordering for correct compilation. No scope creep.

## Issues Encountered
- Pre-existing RingBuffer.PushBackOnZeroCapacity test failure (unrelated to this plan, not investigated -- same as Plan 01)

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Type-safe state machine is now complete: variant state, typed transitions, entry/exit actions
- All state_tag/dispatch_state legacy removed; codebase is clean
- Shadow atomic enum (AppStateTag) maintained by transition_to() for cross-thread reads
- Ready for Phase 14 (variant-transitions refinement) or Phase 15 (runner FSM)

---
*Phase: 13-type-safe-states*
*Completed: 2026-02-28*

## Self-Check: PASSED
