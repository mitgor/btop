---
phase: 23-input-fsm
plan: 02
subsystem: input
tags: [fsm, input-dispatch, menu-active-removal, invalidate-layout, thread-safety]

# Dependency graph
requires:
  - phase: 23-input-fsm
    plan: 01
    provides: "InputStateVar FSM with query functions, transition functions, and FSM-dispatched process()"
provides:
  - "Menu::active atomic bool fully removed (declaration, definition, all reads and writes)"
  - "Menu::invalidate_layout() wrapper for PDA layout invalidation"
  - "InputStateVar as sole authority for input routing (INPUT-05 complete)"
  - "Thread-safe runner check using Runner::pause_output.load() instead of non-atomic FSM read"
  - "INTEG-01: on_enter(Resizing) invalidates layout before menu re-render"
  - "INTEG-02: Runner::pause_output cleared on all PDA-emptying paths (verified)"
affects: [24-tests, 25-cleanup]

# Tech tracking
tech-stack:
  added: []
  patterns: [pause-output-as-menu-signal, invalidate-layout-on-resize]

key-files:
  created: []
  modified:
    - src/btop_menu.hpp
    - src/btop_menu.cpp
    - src/btop.cpp
    - src/btop_draw.cpp
    - src/btop_input.cpp

key-decisions:
  - "Runner::pause_output.load() used as atomic signal for PDA-empty detection in both runner thread (btop.cpp:906) and Input::process() bridge (btop_input.cpp:258)"
  - "Menu::invalidate_layout() wrapper delegates to pda.invalidate_layout() -- keeps PDA file-scoped while exposing layout reset"
  - "btop_input.cpp bridge uses pause_output instead of Menu::active -- maintains FSM transition ownership in Input code"

patterns-established:
  - "pause-output-as-menu-signal: Runner::pause_output serves as atomic cross-thread indicator of menu activity, replacing non-atomic Menu::active"
  - "invalidate-layout-on-resize: on_enter(Resizing) calls Menu::invalidate_layout() before Menu::process() to prevent stale coordinates"

requirements-completed: [INPUT-05, INTEG-01, INTEG-02]

# Metrics
duration: 4min
completed: 2026-03-02
---

# Phase 23 Plan 02: Menu::active Removal and Integration Wiring Summary

**Menu::active atomic bool fully removed, replaced with InputStateVar FSM queries and Runner::pause_output for thread-safe cross-thread checks**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-02T13:14:12Z
- **Completed:** 2026-03-02T13:18:45Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Deleted Menu::active atomic bool declaration, definition, and all writes from btop_menu.hpp/cpp
- Replaced all 4 Menu::active reads in btop.cpp with Input::is_menu_active() (main thread) and Runner::pause_output.load() (runner thread)
- Replaced Menu::active read in btop_draw.cpp with Input::is_menu_active()
- Added Menu::invalidate_layout() wrapper called from on_enter(Resizing) for INTEG-01
- Fixed bridge code in btop_input.cpp to use Runner::pause_output.load() instead of deleted Menu::active
- Verified Runner::pause_output cleared on all 4 PDA-emptying paths (INTEG-02)
- Zero functional references to Menu::active remain in src/ (INPUT-05 complete)

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove Menu::active definition/writes from btop_menu.cpp, add invalidate_layout wrapper** - `165f7c1` (feat)
2. **Task 2: Replace all Menu::active reads in btop.cpp and btop_draw.cpp with FSM queries** - `f8bdedc` (feat)

## Files Created/Modified
- `src/btop_menu.hpp` - Removed atomic<bool> active declaration, #include <atomic>, using std::atomic; added invalidate_layout() declaration
- `src/btop_menu.cpp` - Removed active definition and both writes; added invalidate_layout() wrapper delegating to pda
- `src/btop.cpp` - 3 reads replaced with Input::is_menu_active(), 1 runner-thread read replaced with Runner::pause_output.load(), on_enter(Resizing) calls Menu::invalidate_layout()
- `src/btop_draw.cpp` - calcSizes Menu::active read replaced with Input::is_menu_active()
- `src/btop_input.cpp` - process() bridge uses Runner::pause_output.load() instead of Menu::active for PDA-empty detection

## Decisions Made
- Runner::pause_output.load() was used as the atomic cross-thread signal for PDA-empty detection, replacing Menu::active in both the runner thread (btop.cpp:906) and the Input::process() bridge code (btop_input.cpp:258). This is safe because Menu::process() already sets pause_output.store(false) on the empty-stack path.
- Menu::invalidate_layout() was implemented as a free-function wrapper rather than exposing the PDA directly, keeping the PDA file-scoped within btop_menu.cpp.
- The btop_input.cpp bridge code was updated in Task 2 (found during build) rather than being planned separately, since it was a direct consequence of removing Menu::active.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed Menu::active reference in btop_input.cpp bridge code**
- **Found during:** Task 2 (build after replacing btop.cpp/btop_draw.cpp references)
- **Issue:** btop_input.cpp:258 still read Menu::active to detect PDA empty after Menu::process() returns. This was the Plan 01 bridge code that the Plan 02 spec didn't explicitly list (it only listed btop.cpp and btop_draw.cpp references).
- **Fix:** Replaced `!Menu::active` with `!Runner::pause_output.load()` -- same atomic signal that Menu::process() sets when PDA empties
- **Files modified:** src/btop_input.cpp
- **Verification:** Build succeeds, all tests pass
- **Committed in:** f8bdedc (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Necessary fix for compilation. The btop_input.cpp bridge code was a known Plan 01 artifact that was implicitly part of Menu::active removal. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- InputStateVar is now the sole authority for input routing (INPUT-05)
- Menu::active has zero references anywhere in src/
- All integration wiring complete: INTEG-01 (resize invalidation), INTEG-02 (pause_output clearing), INTEG-03 (proc_filtering sync from Plan 01), INTEG-04 (mouse routing from Plan 01)
- Ready for Phase 24 (tests) and Phase 25 (cleanup)

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 23-input-fsm*
*Completed: 2026-03-02*
