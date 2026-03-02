---
phase: 20-pda-types-skeleton
plan: 01
subsystem: ui
tags: [variant, pda, state-machine, cpp17, googletest]

# Dependency graph
requires: []
provides:
  - "MenuFrameVar variant with 8 typed frame structs"
  - "PDAAction enum (NoChange, Push, Pop, Replace)"
  - "MenuPDA class with push/pop/replace/top/empty/depth and bg lifecycle"
  - "Per-frame mouse_mappings value members"
  - "31 unit tests covering all PDA type requirements"
affects: [21-static-pda-instance, 22-dispatch-switchover]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Variant-based PDA with typed frame structs (menu namespace)"
    - "std::stack with vector backing for PDA stack"
    - "Layout/interaction/mouse_mappings field separation in frame structs"
    - "NSDMI (non-static data member initializers) for all frame fields"

key-files:
  created:
    - src/btop_menu_pda.hpp
    - tests/test_menu_pda.cpp
  modified:
    - tests/CMakeLists.txt

key-decisions:
  - "Placed all types in menu namespace following btop_state.hpp pattern"
  - "Used std::stack<MenuFrameVar, std::vector<MenuFrameVar>> for contiguous memory"
  - "Assert-based preconditions on pop/replace/top rather than exceptions"

patterns-established:
  - "menu::Frame structs as pure data carriers with no behavior methods"
  - "PDAAction return-value pattern for stack operations"
  - "Per-frame mouse_mappings as value members (not shared global)"

requirements-completed: [PDA-01, PDA-02, PDA-03, PDA-05, FRAME-02, FRAME-04, FRAME-05]

# Metrics
duration: 3min
completed: 2026-03-02
---

# Phase 20 Plan 01: PDA Types + Skeleton Summary

**Menu PDA type system with 8 variant-held frame structs, PDAAction enum, and MenuPDA stack class with bg lifecycle**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-02T10:54:42Z
- **Completed:** 2026-03-02T10:58:02Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Created `src/btop_menu_pda.hpp` with complete menu PDA type definitions: 8 frame structs, MenuFrameVar variant, PDAAction enum, and MenuPDA class
- All frame structs have layout/interaction/mouse_mappings field separation with in-class member initializers
- MenuPDA class provides push/pop/replace/top/empty/depth operations with assert-based preconditions and bg string lifecycle management
- 31 comprehensive unit tests covering variant alternatives, PDA stack operations, bg lifecycle, frame defaults, and per-frame mouse mapping ownership
- Full test suite (297 tests) passes with zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Create btop_menu_pda.hpp with all PDA types** - `391ccda` (feat)
2. **Task 2: Create PDA unit tests and wire into test build** - `4670b3d` (test)

_Note: TDD tasks followed RED-GREEN cycle: failing test, then implementation, then comprehensive tests._

## Files Created/Modified
- `src/btop_menu_pda.hpp` - Menu PDA type definitions: 8 frame structs, MenuFrameVar variant, PDAAction enum, MenuPDA class
- `tests/test_menu_pda.cpp` - 31 unit tests for PDA types and operations
- `tests/CMakeLists.txt` - Added test_menu_pda.cpp to btop_test sources

## Decisions Made
- Placed all types in `menu` namespace following the same pattern as `state` namespace in btop_state.hpp
- Used `std::stack<MenuFrameVar, std::vector<MenuFrameVar>>` for contiguous memory backing (avoids deque overhead)
- Used assert-based preconditions on pop/replace/top rather than exceptions (matches project convention)
- Frame structs are pure data carriers with no behavior methods (behavior arrives in Phase 21+)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All PDA types are defined and tested, ready for Phase 21 (static PDA instance and migration wrappers)
- MenuFrameVar variant can be used as the type-safe replacement for menuMask/currentMenu/menuFunc
- PDAAction enum establishes the return-value pattern for transition functions
- No blockers or concerns

## Self-Check: PASSED

- FOUND: src/btop_menu_pda.hpp
- FOUND: tests/test_menu_pda.cpp
- FOUND: 20-01-SUMMARY.md
- FOUND: commit 391ccda (Task 1)
- FOUND: commit 4670b3d (Task 2)

---
*Phase: 20-pda-types-skeleton*
*Completed: 2026-03-02*
