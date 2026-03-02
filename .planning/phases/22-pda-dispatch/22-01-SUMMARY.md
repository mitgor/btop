---
phase: 22-pda-dispatch
plan: 01
subsystem: menu
tags: [pda, visitor-pattern, state-machine, c++20, std-visit, variant]

# Dependency graph
requires:
  - phase: 20-pda-types-skeleton
    provides: "MenuFrameVar, PDAAction enum, MenuPDA class, frame structs"
  - phase: 21-static-local-migration
    provides: "Frame-based state in handlers, single-writer wrappers, PDA seeding in process()"
provides:
  - "PDAResult struct with action + optional<MenuFrameVar> + rendered flag"
  - "All 8 handler functions accepting frame-by-reference and returning PDAResult"
  - "MenuVisitor struct dispatching std::visit to all 8 handlers"
  - "dispatch_legacy compatibility bridge for process()"
affects: [22-02-pda-dispatch, 23-input-fsm]

# Tech tracking
tech-stack:
  added: [std-optional]
  patterns: [visitor-dispatch, return-value-mutations, compatibility-bridge]

key-files:
  created: []
  modified:
    - src/btop_menu_pda.hpp
    - src/btop_menu.cpp
    - tests/test_menu_pda.cpp

key-decisions:
  - "dispatch_legacy bridge translates PDAResult to int codes, keeping process() unchanged for Plan 02"
  - "Replace actions applied inside dispatch_legacy with menuMask/currentMenu sync for correct re-dispatch"
  - "Handler rendered flag replaces int retval pattern -- bool did_render tracks overlay writes"

patterns-established:
  - "Handler signature: PDAResult handler(string_view key, FrameType& frame)"
  - "Return-value mutations: handlers return PDAAction + frame payload, caller applies stack ops"
  - "MenuVisitor: struct with operator() per frame type for std::visit dispatch"

requirements-completed: [PDA-04, PDA-06]

# Metrics
duration: 6min
completed: 2026-03-02
---

# Phase 22 Plan 01: PDA Dispatch Handlers Summary

**PDAResult return type on all 8 menu handlers with MenuVisitor dispatch and dispatch_legacy compatibility bridge**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-02T12:30:30Z
- **Completed:** 2026-03-02T12:37:09Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Added PDAResult struct (action + optional frame + rendered flag) to btop_menu_pda.hpp
- Converted all 8 handler signatures from `int(const string&)` to `PDAResult(string_view, FrameType&)`
- No handler directly mutates PDA stack -- all mutations deferred via PDAResult return values
- mainMenu returns Replace with OptionsFrame/HelpFrame for menu transitions (PDA-06)
- signalChoose/signalSend return Replace with SignalReturnFrame on kill error, Pop on success
- Created MenuVisitor struct dispatching std::visit to all 8 handlers (PDA-04 foundation)
- Added dispatch_legacy bridge translating PDAResult to legacy int codes for process()
- Deleted menuFunc vector -- dispatch now goes through visitor pattern
- Added 5 PDAResult unit tests; all 312 tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Add PDAResult struct and unit tests** - `cad1eb2` (test)
2. **Task 2: Convert all 8 handler signatures to PDAResult and create MenuVisitor** - `1f9e1d4` (feat)

## Files Created/Modified
- `src/btop_menu_pda.hpp` - Added PDAResult struct with action/next_frame/rendered fields, added #include <optional>
- `src/btop_menu.cpp` - Converted 8 handler signatures, added MenuVisitor, dispatch_legacy bridge, deleted menuFunc
- `tests/test_menu_pda.cpp` - Added 5 PDAResult tests (default construction, Pop, Replace, Push, rendered flag)

## Decisions Made
- dispatch_legacy bridge approach chosen to keep process() unchanged -- Plan 02 will replace process() entirely
- Replace actions applied inside dispatch_legacy (safe because visitor has already returned) with menuMask/currentMenu sync for correct re-dispatch
- Handler rendered flag (bool did_render) replaces the legacy int retval pattern (Changed/NoChange)
- string_view key parameter works naturally with is_in(), starts_with(), ==, at(), back(); explicit std::string() for stoi() and messageBox.input()

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All 8 handlers return PDAResult with deferred mutations -- ready for Plan 02 to replace process() dispatch
- dispatch_legacy bridge cleanly separable -- Plan 02 deletes it and handles PDAResult directly
- MenuVisitor provides the std::visit entry point that Plan 02 will use

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 22-pda-dispatch*
*Completed: 2026-03-02*
