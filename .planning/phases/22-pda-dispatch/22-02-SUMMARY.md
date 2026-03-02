---
phase: 22-pda-dispatch
plan: 02
subsystem: menu
tags: [pda, std-visit, dispatch-loop, non-recursive, c++17, variant]

# Dependency graph
requires:
  - phase: 22-pda-dispatch
    plan: 01
    provides: "PDAResult struct, MenuVisitor, 8 handler functions returning PDAResult, dispatch_legacy bridge"
  - phase: 21-static-local-migration
    provides: "Frame-based state in handlers, PDA bg lifecycle, single-writer wrappers"
provides:
  - "Non-recursive process() loop dispatching via std::visit on PDA top frame"
  - "show() pushes frames directly via pda.push() (no menuMask wrappers)"
  - "Complete deletion of menuMask, currentMenu, menuFunc, menuReturnCodes, dispatch_legacy, single-writer wrappers"
  - "Mouse mappings copied from active frame to global Menu::mouse_mappings after dispatch"
  - "Clean btop_menu.hpp header without bitset, menuMask, or output extern"
affects: [23-input-fsm, 25-cleanup]

# Tech tracking
tech-stack:
  added: []
  patterns: [non-recursive-dispatch-loop, pda-sole-authority, visitor-action-loop]

key-files:
  created: []
  modified:
    - src/btop_menu.cpp
    - src/btop_menu.hpp

key-decisions:
  - "Mouse mappings copied to global only on NoChange (when staying on current frame); cleared on Pop/Replace"
  - "SizeError override checked only on first_call, not on re-dispatch after Pop/Replace (matches original behavior)"
  - "current_key reset to empty string after Pop/Replace/Push so next frame gets a redraw call"
  - "Removed unused 'using std::ref' after menuFunc vector deletion"

patterns-established:
  - "PDA dispatch loop: while(true) { visit -> switch(action) -> apply } replaces recursive process()"
  - "show() creates frame and pushes to PDA, then calls process() -- no menuMask sync needed"
  - "bg lifecycle: clear_bg() on Pop and Replace in process() loop; set_bg() in handler redraw blocks"

requirements-completed: [PDA-04, PDA-06]

# Metrics
duration: 4min
completed: 2026-03-02
---

# Phase 22 Plan 02: PDA Dispatch Loop Summary

**Non-recursive process() loop with std::visit dispatch, complete deletion of menuMask/currentMenu/menuFunc/dispatch_legacy legacy infrastructure**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-02T12:40:21Z
- **Completed:** 2026-03-02T12:44:21Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Replaced recursive process() with non-recursive while-loop that dispatches via std::visit(MenuVisitor{key}, pda.top()) and applies PDAActions iteratively
- Deleted all legacy dispatch infrastructure: menuMask bitset, currentMenu int, menuReturnCodes enum, dispatch_legacy() bridge, menu_open/menu_close_current/menu_clear_all wrappers
- Rewrote show() to push frames directly via pda.push() instead of through menu_open() wrapper
- Cleaned btop_menu.hpp: removed extern bitset<8> menuMask, extern string output (dead declaration), #include <bitset>, using std::bitset
- PDA is now the sole dispatch authority (PDA-04 fully delivered)
- All 312 tests pass; btop compiles cleanly with zero warnings

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite process() as non-recursive PDA dispatch loop** - `9d00b6b` (feat)
2. **Task 2: Clean btop_menu.hpp header and verify legacy deletion** - `a750bda` (chore)

## Files Created/Modified
- `src/btop_menu.cpp` - Replaced process() with non-recursive dispatch loop, replaced show() with direct pda.push(), deleted dispatch_legacy/menuMask/currentMenu/menuReturnCodes/wrappers, removed unused `using std::ref`
- `src/btop_menu.hpp` - Removed menuMask extern, output extern, bitset include/using; updated PDA comment

## Decisions Made
- Mouse mappings from the active frame are copied to global Menu::mouse_mappings only on NoChange (when the handler stays on the current frame). On Pop/Replace/Push, mouse_mappings are cleared and the next iteration sets them from the new top frame.
- SizeError terminal-too-small override is checked only on first_call (initial entry into process()), not on re-dispatch after Pop/Replace. This matches the original behavior where the size check happened before the first dispatch.
- After Pop/Replace/Push, current_key is reset to empty string so the next frame receives a "redraw" dispatch rather than the original key input.
- Removed `using std::ref` since it was only used for the deleted menuFunc vector.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Removed unused `using std::ref` declaration**
- **Found during:** Task 1
- **Issue:** After deleting menuFunc vector (which was the sole user of std::ref), the using declaration became unused
- **Fix:** Removed `using std::ref;` line
- **Files modified:** src/btop_menu.cpp
- **Verification:** Compiles cleanly
- **Committed in:** 9d00b6b (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Trivial cleanup of dead using-declaration. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- PDA is the sole dispatch authority -- ready for Phase 23 (Input FSM) which will take over Menu::active management
- Menus enum kept in btop_menu.hpp for external callers (btop_input.cpp) -- Phase 25 cleanup will replace with typed API
- All handlers return PDAResult with deferred mutations -- clean foundation for further state machine evolution

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 22-pda-dispatch*
*Completed: 2026-03-02*
