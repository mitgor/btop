---
phase: 21-static-local-migration
plan: 01
subsystem: menu
tags: [pda, pushdown-automaton, invalidate-layout, frame-structs, state-machine]

# Dependency graph
requires:
  - phase: 20-pda-types-skeleton
    provides: "MenuPDA class, 8 frame structs, MenuFrameVar variant, PDAAction enum"
provides:
  - "invalidate_layout() method on all 8 frame structs (FRAME-03)"
  - "MenuPDA::invalidate_layout() dispatching via std::visit"
  - "File-scope menu::MenuPDA pda instance in btop_menu.cpp"
  - "Single-writer wrappers: menu_open, menu_close_current, menu_clear_all"
  - "Adapted process()/show() using PDA wrappers for all menuMask mutations"
affects: [21-02-static-local-migration, 22-pda-dispatch]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Single-writer wrapper pattern for menuMask/PDA sync"
    - "invalidate_layout() zeros layout fields, preserves interaction fields"
    - "assert(menuMask.none() == pda.empty()) invariant guard at sync points"

key-files:
  created: []
  modified:
    - src/btop_menu_pda.hpp
    - src/btop_menu.cpp
    - src/btop_menu.hpp
    - tests/test_menu_pda.cpp

key-decisions:
  - "Coexistence of bg and pda.bg() during migration -- both cleared in parallel until Plan 02 migrates functions"
  - "PDA seeded lazily on first dispatch in process() via switch on currentMenu"
  - "menu_close_current omits invariant assert since process() may close and reopen in sequence"

patterns-established:
  - "Single-writer wrappers: all menuMask mutations go through menu_open/menu_close_current/menu_clear_all"
  - "invalidate_layout() convention: zeros layout fields under '--- Layout fields ---' comment, clears mouse_mappings, preserves interaction fields"

requirements-completed: [FRAME-03]

# Metrics
duration: 4min
completed: 2026-03-02
---

# Phase 21 Plan 01: Static Local Migration - PDA Infrastructure Summary

**invalidate_layout() on all 8 frame structs with file-scope PDA instance and single-writer wrappers adapting process()/show()**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-02T11:28:13Z
- **Completed:** 2026-03-02T11:32:22Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- All 8 frame structs have invalidate_layout() methods that zero layout fields and clear mouse_mappings while preserving interaction fields
- MenuPDA::invalidate_layout() dispatches via std::visit to the top frame, safe no-op on empty stack
- File-scope menu::MenuPDA pda instance established in btop_menu.cpp
- Single-writer wrappers (menu_open, menu_close_current, menu_clear_all) keep menuMask and PDA stack in sync
- process() and show() fully adapted to use wrappers for all menuMask mutations
- assert(menuMask.none() == pda.empty()) invariant guard active at key sync points
- 307 tests pass (297 baseline + 10 new InvalidateLayout tests), btop binary compiles cleanly

## Task Commits

Each task was committed atomically:

1. **Task 1: Add invalidate_layout() to frame structs and MenuPDA** - `71a3764` (test: RED), `eae50c3` (feat: GREEN)
2. **Task 2: Add file-scope PDA instance and single-writer wrappers** - `befa68c` (feat)

**Plan metadata:** [pending] (docs: complete plan)

_Note: Task 1 used TDD with separate RED and GREEN commits._

## Files Created/Modified
- `src/btop_menu_pda.hpp` - Added invalidate_layout() to all 8 frame structs and MenuPDA class
- `src/btop_menu.cpp` - Added PDA include, file-scope pda instance, single-writer wrappers, adapted process()/show()
- `src/btop_menu.hpp` - Added forward declaration comment noting PDA is file-scope in btop_menu.cpp
- `tests/test_menu_pda.cpp` - Added 10 InvalidateLayout tests covering all frame types and PDA dispatch

## Decisions Made
- Coexistence of `bg` and `pda.bg()` during migration: both cleared in parallel until Plan 02 migrates individual menu functions from `bg` to `pda.bg()`
- PDA seeded lazily on first dispatch in process() via switch on currentMenu rather than eagerly, to handle existing menuMask-set-before-process patterns
- menu_close_current omits the invariant assert since process() may close a menu and recursively call itself to process the next menu in sequence

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- PDA infrastructure is in place for Plan 02 to migrate individual menu functions to read/write from frame structs
- Each menu function can now reach into file-scope `pda` to get its typed frame via `std::get<FrameType>(pda.top())`
- The single-writer wrappers ensure menuMask and PDA remain in sync throughout the migration

## Self-Check: PASSED

All files exist, all commits verified (71a3764, eae50c3, befa68c).

---
*Phase: 21-static-local-migration*
*Completed: 2026-03-02*
