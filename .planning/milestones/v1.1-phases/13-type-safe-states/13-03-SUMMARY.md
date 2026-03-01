---
phase: 13-type-safe-states
plan: 03
subsystem: state-machine
tags: [variant, state-machine, signal-handling, clean-quit, suspend-resume]

# Dependency graph
requires:
  - phase: 13-type-safe-states P02
    provides: "variant-based dispatch with entry/exit actions, transition_to(), on_enter/on_exit"
provides:
  - "Fixed clean_quit re-entrancy guard using static bool instead of shadow atomic"
  - "on_event(Sleeping, Resume) handler routing through Resizing for full redraw"
  - "Defense-in-depth outer loop exit for terminal states"
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Static bool re-entrancy guard for functions called from both signal context and state machine"
    - "Specific on_event overloads take priority over catch-all auto template"

key-files:
  created: []
  modified:
    - src/btop.cpp

key-decisions:
  - "Static bool re-entrancy guard chosen over atomic because transition_to sets shadow atomic before on_enter"
  - "Resume from Sleeping routes through Resizing state for full redraw via on_enter(Resizing)"
  - "Outer loop exit check is defense-in-depth since clean_quit calls _Exit/_quick_exit"

patterns-established:
  - "Re-entrancy guard pattern: use static bool when shadow atomic conflicts with state machine ordering"

requirements-completed: [TRANS-03]

# Metrics
duration: 2min
completed: 2026-02-28
---

# Phase 13 Plan 03: Gap Closure Summary

**Fixed Ctrl+C hang via static bool re-entrancy guard and resume-no-redraw via on_event(Sleeping, Resume) -> Resizing**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-28T17:29:16Z
- **Completed:** 2026-02-28T17:30:47Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Fixed Ctrl+C hang caused by transition_to() setting shadow atomic to Quitting before on_enter calls clean_quit
- Fixed missing display redraw after Ctrl+Z/fg by adding specific on_event(Sleeping, Resume) handler
- Added defense-in-depth outer loop exit for terminal states (Quitting/Error)

## Task Commits

Each task was committed atomically:

1. **Task 1: Fix Ctrl+C hang -- clean_quit re-entrancy guard and main loop exit** - `eac8ef6` (fix)
2. **Task 2: Fix resume-no-redraw -- add on_event(Sleeping, Resume) handler** - `1e34140` (fix)

## Files Created/Modified
- `src/btop.cpp` - Fixed clean_quit re-entrancy guard (static bool), added outer loop terminal state exit, added on_event(Sleeping, Resume) -> Resizing handler

## Decisions Made
- Used static bool re-entrancy guard instead of shadow atomic because transition_to() stores AppStateTag::Quitting to the shadow atomic at line 1010 before calling on_enter(Quitting) at line 1017, which meant the old atomic-based guard always saw Quitting and returned early
- Routed Sleeping+Resume through Resizing state rather than directly to Running, because on_enter(Resizing) triggers the full redraw pipeline (term_resize, Draw::calcSizes, Runner::run("all", true, true))
- Outer loop exit is defense-in-depth since clean_quit() typically calls _Exit()/_quick_exit() inside the drain loop

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- Pre-existing test failure `RingBuffer.PushBackOnZeroCapacity` (1 of 210 tests) confirmed as pre-existing and unrelated to our changes. All 22 transition tests pass, 209 of 210 total tests pass.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Both UAT-reported bugs (Ctrl+C hang, resume-no-redraw) are fixed
- All state machine transition tests pass (22/22)
- Phase 13 gap closure complete; type-safe state machine is fully functional

## Self-Check: PASSED

- FOUND: 13-03-SUMMARY.md
- FOUND: eac8ef6 (Task 1 commit)
- FOUND: 1e34140 (Task 2 commit)

---
*Phase: 13-type-safe-states*
*Completed: 2026-02-28*
