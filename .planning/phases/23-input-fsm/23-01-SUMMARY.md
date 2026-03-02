---
phase: 23-input-fsm
plan: 01
subsystem: input
tags: [std-variant, fsm, input-dispatch, state-machine, c++17]

# Dependency graph
requires:
  - phase: 22-pda-dispatch
    plan: 02
    provides: "Non-recursive process() loop with PDA as sole dispatch authority, Menu::active still written by process()"
provides:
  - "InputStateVar variant type with Normal/Filtering/MenuActive states"
  - "File-scoped fsm_state instance as sole input routing authority"
  - "Typed transition functions: enter_filtering/exit_filtering/enter_menu/exit_menu"
  - "Query functions: is_menu_active()/is_filtering() for external code"
  - "Unified Input::process() entry point that FSM-dispatches internally"
  - "Simplified btop.cpp main loop with single Input::process(key) call"
affects: [23-input-fsm/02, 24-tests, 25-cleanup]

# Tech tracking
tech-stack:
  added: []
  patterns: [input-fsm-dispatch, typed-state-transitions, query-function-wrappers]

key-files:
  created: []
  modified:
    - src/btop_input.hpp
    - src/btop_input.cpp
    - src/btop.cpp

key-decisions:
  - "FSM transitions owned by Input code -- enter_menu() called before Menu::show(), exit_menu() called after Menu::process() when PDA empties"
  - "Menu::active temporarily read in process() to detect PDA empty after Menu::process() -- removed in Plan 02"
  - "Filtering enter/exit paths replaced with transition functions in Task 1 (needed for old_filter deletion to compile)"

patterns-established:
  - "InputStateVar pattern: file-scoped variant with query wrappers for external reads, transition functions for state changes"
  - "FSM dispatch in process(): MenuActive check first, then holds_alternative<Filtering> for routing, Normal falls through"
  - "enter_menu() before Menu::show() -- FSM transitions before menu display, not inside Menu code"

requirements-completed: [INPUT-01, INPUT-02, INPUT-03, INPUT-04, INPUT-06, INTEG-03, INTEG-04]

# Metrics
duration: 3min
completed: 2026-03-02
---

# Phase 23 Plan 01: Input FSM Core Summary

**InputStateVar variant with Normal/Filtering/MenuActive states, FSM-dispatched process()/get(), unified main loop entry point**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-02T13:07:07Z
- **Completed:** 2026-03-02T13:10:38Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Defined InputStateVar std::variant with three state structs (Normal, Filtering with old_filter member, MenuActive) in btop_input.hpp
- Implemented file-scoped fsm_state instance with typed transition functions and query wrappers in btop_input.cpp
- Deleted file-scoped old_filter global -- replaced by Filtering struct member with proper lifetime
- Rewrote Input::process() to FSM-dispatch: MenuActive delegates to Menu::process(), Filtering handles filter keys, Normal handles all other keys
- Replaced mouse routing in Input::get() to use is_menu_active()/is_filtering() instead of Menu::active/Config::proc_filtering
- Simplified btop.cpp main loop from if/else dispatch to single Input::process(key) call
- Added enter_menu() before all 8 Menu::show() call sites for FSM transition before display

## Task Commits

Each task was committed atomically:

1. **Task 1: Define InputStateVar types, transition functions, and query wrappers** - `86ffa17` (feat)
2. **Task 2: Rewrite process()/get() with FSM dispatch, replace main loop** - `b4ed61d` (feat)

## Files Created/Modified
- `src/btop_input.hpp` - Added input_state namespace with Normal/Filtering/MenuActive structs, InputStateVar variant type alias, query and transition function declarations, variant include
- `src/btop_input.cpp` - Added fsm_state instance, implemented transition/query functions, deleted old_filter global, rewrote process() with FSM dispatch, updated get() mouse routing
- `src/btop.cpp` - Replaced main loop if(Menu::active)/else dispatch with single Input::process(key) call

## Decisions Made
- FSM transitions are owned by Input code: enter_menu() is called before Menu::show(), and exit_menu() is called after Menu::process() returns when Menu::active is false (PDA empty). This keeps FSM ownership centralized in btop_input.cpp.
- Menu::active is temporarily read inside process() to detect when the PDA empties after Menu::process() returns. This is a bridge until Plan 02 removes Menu::active entirely.
- Filtering enter/exit paths were replaced with transition function calls in Task 1 (rather than Task 2) because deleting the old_filter global required the transition functions to be used immediately for compilation.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Replaced filtering paths with transition functions in Task 1**
- **Found during:** Task 1
- **Issue:** Deleting the file-scoped old_filter global caused 4 compilation errors in process() where old_filter was still referenced. Task 1's done criteria requires "Build succeeds with zero errors."
- **Fix:** Replaced the filtering enter path (lines 345-348) with enter_filtering() call and both exit paths (enter/down accept, escape/mouse_click reject) with exit_filtering(true/false) calls in Task 1 instead of deferring to Task 2.
- **Files modified:** src/btop_input.cpp
- **Verification:** Build succeeds with zero errors
- **Committed in:** 86ffa17 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Minor task boundary shift -- filtering path replacement pulled from Task 2 into Task 1 for compilation. Task 2 then added the remaining FSM dispatch changes (MenuActive routing, enter_menu() calls, mouse routing, main loop). No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- InputStateVar FSM is the sole routing authority for input dispatch
- Menu::active still written by btop_menu.cpp (lines 1828, 1838) and read in btop.cpp (4 locations) and btop_draw.cpp (1 location) -- Plan 02 removes these
- Config::proc_filtering still set by transition functions for display purposes (INTEG-03 satisfied)
- Ready for Plan 02 to remove Menu::active atomic bool and remaining boolean flag reads

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 23-input-fsm*
*Completed: 2026-03-02*
