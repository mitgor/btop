---
phase: 12-extract-transitions
plan: 02
subsystem: core
tags: [state-machine, dispatch, main-loop, event-driven, c++20]

requires:
  - phase: 12-extract-transitions
    provides: dispatch_event(), on_event() overloads, state_tag types
provides:
  - Event-driven main loop using dispatch_event() for signal event routing
  - process_accumulated_state() for multi-step state actions
  - Removal of all process_signal_event() overloads
affects: [13-entry-exit-actions, 14-runner-fsm]

tech-stack:
  added: []
  patterns: [event-driven main loop, accumulated state processing, dispatch-then-process]

key-files:
  created: []
  modified:
    - src/btop.cpp

key-decisions:
  - "process_accumulated_state() defined before btop_main, takes Cli::Cli& for init_config access"
  - "Resume event calls _resume() in drain loop (before dispatch_event) to avoid test side effects"
  - "Timer tick and key input stay inline (not dispatched via dispatch_event) per research Pitfall 5"
  - "Reloading chains to Resizing in process_accumulated_state (no return between them)"
  - "Running state guard added to input polling for-loop condition"

patterns-established:
  - "Dispatch-then-process: drain loop dispatches events for state transitions, process_accumulated_state handles multi-step actions"
  - "Resume special case: terminal re-init before state dispatch to preserve order"

requirements-completed: [TRANS-04, EVENT-04]

duration: 12min
completed: 2026-02-28
---

# Plan 12-02: Event-Driven Main Loop Summary

**Main loop rewired from process_signal_event + if/else if chain to dispatch_event() + process_accumulated_state(), net 19-line reduction**

## Performance

- **Duration:** 12 min
- **Started:** 2026-02-28
- **Completed:** 2026-02-28
- **Tasks:** 2 (1 auto + 1 checkpoint auto-approved)
- **Files modified:** 1

## Accomplishments
- process_accumulated_state() handles Error, Quitting, Sleeping, Reloading->Resizing chain
- Main loop event drain uses dispatch_event() instead of std::visit + process_signal_event
- All 8 process_signal_event overloads removed (replaced by on_event + dispatch_event)
- Resume event correctly calls _resume() for terminal re-initialization
- Net 19-line reduction while preserving all behavior
- Clean build with zero warnings, 170/171 tests pass (1 pre-existing RingBuffer failure)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add process_accumulated_state and rewire main loop** - `8592eb8` (feat)
2. **Task 2: Verify event-driven main loop** - Auto-approved checkpoint

## Files Created/Modified
- `src/btop.cpp` - Rewired main loop, added process_accumulated_state(), removed process_signal_event overloads

## Decisions Made
- process_accumulated_state() takes Cli::Cli& parameter since cli is local to btop_main
- Resume event handled specially in drain loop (calls _resume() before dispatch) to keep on_event pure/testable
- Timer tick and key input remain inline in main loop (not dispatched via dispatch_event) per research recommendations
- Comments updated to reference dispatch_event instead of process_signal_event

## Deviations from Plan

### Auto-fixed Issues

**1. process_accumulated_state needs cli access**
- **Found during:** Task 1 (build error)
- **Issue:** cli is a local variable in btop_main(), not accessible from static function defined before it
- **Fix:** Moved process_accumulated_state() to right before btop_main(), added Cli::Cli& parameter
- **Files modified:** src/btop.cpp
- **Verification:** Build succeeds, all tests pass
- **Committed in:** 8592eb8

**2. Runner::screen_buffer not visible at original definition site**
- **Found during:** Task 1 (build error)
- **Issue:** screen_buffer is in Runner namespace defined later in file
- **Fix:** Moving process_accumulated_state after Runner namespace definition resolved both issues
- **Files modified:** src/btop.cpp
- **Verification:** Build succeeds
- **Committed in:** 8592eb8

**3. Resume event needs _resume() call for terminal re-init**
- **Found during:** Task 1 (behavioral analysis)
- **Issue:** Original process_signal_event(Resume) called _resume() (Term::init + term_resize), but pure on_event cannot have side effects without breaking tests
- **Fix:** Added Resume check in drain loop (std::holds_alternative) to call _resume() before dispatch
- **Files modified:** src/btop.cpp
- **Verification:** Build succeeds, _resume no longer unused, behavior preserved
- **Committed in:** 8592eb8

---

**Total deviations:** 3 auto-fixed (2 build errors, 1 behavioral correctness)
**Impact on plan:** Essential for correctness. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviations.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Main loop is fully event-driven via dispatch_event()
- on_event() overloads define pure state transitions
- process_accumulated_state() handles multi-step actions
- Ready for Phase 13 entry/exit actions

---
*Phase: 12-extract-transitions*
*Completed: 2026-02-28*
