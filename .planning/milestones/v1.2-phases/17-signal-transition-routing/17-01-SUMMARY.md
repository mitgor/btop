---
phase: 17-signal-transition-routing
plan: 01
subsystem: state-machine
tags: [fsm, signal-handling, atomic, SIGTERM, event-queue, shadow-atomic]

# Dependency graph
requires:
  - phase: 16-runner-error-path-purity
    provides: "FSM transition_to() framework, event queue, on_event/on_enter/on_exit dispatch"
provides:
  - "transition_to() is the ONLY writer of Global::app_state (grep-verifiable)"
  - "SIGTERM routed through event queue like all other signals"
  - "term_resize() returns bool, no shadow writes"
  - "clean_quit() shadow-pure (no app_state writes)"
  - "Input and Menu quit paths route through event::Quit"
affects: [18-hygiene, 19-measurement]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Return-value signaling: term_resize() returns bool instead of writing shadow atomic"
    - "Event-queue quit routing: all user-initiated quits push event::Quit instead of calling clean_quit directly"
    - "Signal fallthrough: SIGTERM shares case with SIGINT in _signal_handler switch"

key-files:
  created: []
  modified:
    - src/btop.cpp
    - src/btop_shared.hpp
    - src/btop_tools.cpp
    - src/btop_input.cpp
    - src/btop_menu.cpp

key-decisions:
  - "Removed clean_quit() shadow write: safe because transition_to() sets state before on_enter calls clean_quit, and init-time calls have no cross-thread readers"
  - "SIGTERM uses SIGINT fallthrough in switch: cleaner than separate case, same behavior"
  - "term_resize() 'q' key pushes event::Quit instead of calling clean_quit: ensures FSM handles all quit transitions"

patterns-established:
  - "Single-writer invariant: Global::app_state.store() appears exactly once in codebase (in transition_to)"
  - "Event-queue mediation: cross-thread and signal-to-main communication always via event queue"

requirements-completed: [PURE-05, PURE-06]

# Metrics
duration: 8min
completed: 2026-03-01
---

# Phase 17 Plan 01: Signal & Transition Routing Summary

**Eliminated all shadow atomic bypasses so transition_to() is the sole writer of Global::app_state, with SIGTERM routed through event queue**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-01T21:32:49Z
- **Completed:** 2026-03-01T21:40:33Z
- **Tasks:** 3
- **Files modified:** 5

## Accomplishments
- Exactly ONE app_state.store() call remains across entire codebase (in transition_to) -- grep-verified
- SIGTERM now routed through event queue via _signal_handler, matching SIGINT/SIGTSTP/SIGWINCH/SIGUSR2
- term_resize() returns bool and contains zero shadow writes; callers use return value for state decisions
- clean_quit() contains zero shadow writes; state already set by transition_to() before on_enter calls it
- All user-initiated quit paths (Input 'q' key, Menu Quit option) route through event::Quit
- Zero ASan/UBSan/TSan findings from changes

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove shadow writes from term_resize() and runner coreNum path** - `8da0866` (refactor)
2. **Task 2: Remove shadow write from clean_quit() and route SIGTERM through event system** - `b10a3ff` (feat)
3. **Task 3: Grep-verify and sanitizer sweeps** - verification only, no code changes

**Plan metadata:** `64157c5` (docs: complete plan)

## Files Created/Modified
- `src/btop.cpp` - term_resize() returns bool, removed 4 shadow writes, added SIGTERM registration and case, removed init compare_exchange, updated main loop fallback to use return value
- `src/btop_shared.hpp` - Changed term_resize() declaration from void to bool
- `src/btop_tools.cpp` - Removed stale compare_exchange_strong in Term::init()
- `src/btop_input.cpp` - Input quit path pushes event::Quit instead of calling clean_quit
- `src/btop_menu.cpp` - Menu quit path pushes event::Quit instead of calling clean_quit, added btop_input.hpp include

## Decisions Made
- Removed clean_quit() shadow write: safe because transition_to() already sets app_state before on_enter calls clean_quit. For init-time fatal errors, no cross-thread reader exists yet. For Runner::stop() fatal paths, runner_state_tag handles coordination.
- SIGTERM uses SIGINT case fallthrough in the switch statement rather than a separate case -- same behavior, cleaner code.
- term_resize() 'q' key path pushes event::Quit instead of calling clean_quit directly -- ensures all quit transitions flow through the FSM.
- Left btop_config.cpp clean_quit() call as-is: it's called from config validation during reload, already inside a transition. Since clean_quit() no longer writes the shadow, no bypass occurs.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- Pre-existing test failure: RingBuffer.PushBackOnZeroCapacity (1/266 tests) -- unrelated to our changes, present before and after. Not in scope for this phase.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Phase 17 complete: all shadow atomic bypasses eliminated
- transition_to() is the verified single writer of Global::app_state
- Ready for Phase 18 (hygiene) or Phase 19 (measurement)
- No blockers or concerns

## Self-Check: PASSED

All modified files exist on disk. All task commits (8da0866, b10a3ff) verified in git log.

---
*Phase: 17-signal-transition-routing*
*Completed: 2026-03-01*
