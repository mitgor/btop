---
phase: 21-static-local-migration
plan: 02
subsystem: menu
tags: [pda, static-local-migration, frame-structs, bg-lifecycle, mouse-mappings]

# Dependency graph
requires:
  - phase: 21-static-local-migration
    plan: 01
    provides: "File-scope MenuPDA pda instance, single-writer wrappers, adapted process()/show()"
  - phase: 20-pda-types-skeleton
    provides: "MenuPDA class, 8 frame structs with value-initialized fields"
provides:
  - "All 8 menu functions migrated to frame-based state via auto& aliases"
  - "Zero mutable function-static locals in menu function bodies"
  - "bg.empty() sentinel pattern fully eliminated"
  - "All bg reads/writes routed through pda.bg()/pda.set_bg()/pda.clear_bg()"
  - "Per-frame mouse_mappings for signalChoose, mainMenu, optionsMenu"
  - "pda.replace() for mainMenu Switch paths (Main->Options, Main->Help)"
  - "File-scope string bg removed (fully replaced by PDA bg lifecycle)"
affects: [22-pda-dispatch]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "auto& alias pattern: auto& x = frame.x; keeps function body unchanged"
    - "Local bg string built then moved into pda.set_bg(std::move(local_bg))"
    - "pda.replace() for depth-preserving menu transitions (Main->Options/Help)"
    - "Frame constructor value-initialization replaces bg.empty() sentinel resets"

key-files:
  created: []
  modified:
    - src/btop_menu.cpp

key-decisions:
  - "Moved Theme::updateThemes() into optionsMenu redraw block (guarded by pda.bg().empty()) instead of standalone sentinel"
  - "Kept menuMask.set(SignalReturn) in signalChoose/signalSend as-is -- PDA catch-up handled by process() lazy seeding"
  - "Removed file-scope string bg since all functions now use PDA bg lifecycle exclusively"

patterns-established:
  - "auto& alias pattern for frame struct access: minimal code churn, function body unchanged"
  - "Local string accumulation + pda.set_bg(std::move()) for complex bg construction"
  - "Per-frame mouse_mappings: each function writes to frame.mouse_mappings instead of file-scope map"

requirements-completed: [FRAME-01]

# Metrics
duration: 6min
completed: 2026-03-02
---

# Phase 21 Plan 02: Static Local Migration - Menu Function Migration Summary

**All 8 menu functions migrated from function-static locals to per-frame state via auto& aliases, eliminating bg.empty() sentinel and file-scope bg string**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-02T11:37:31Z
- **Completed:** 2026-03-02T11:43:15Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- All 8 menu functions (sizeError, signalSend, signalReturn, signalChoose, reniceMenu, helpMenu, mainMenu, optionsMenu) retrieve typed frames via std::get<FrameType>(pda.top())
- Zero mutable function-static locals remain in any menu function body (verified by grep)
- bg.empty() sentinel pattern completely eliminated from all functions
- All bg reads/writes routed through pda.bg()/pda.set_bg()/pda.clear_bg()
- mainMenu Switch paths (Options, Help) use pda.replace() for depth-preserving transitions
- File-scope `string bg` declaration removed -- PDA owns bg lifecycle exclusively
- 307 tests pass, btop binary compiles cleanly, zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate all 8 menu functions from static locals to frame struct access** - `20d2bda` (feat)
2. **Task 2: Verify static local elimination and final cleanup** - verification only, no code changes needed

**Plan metadata:** (pending docs commit)

## Files Created/Modified
- `src/btop_menu.cpp` - Migrated all 8 menu functions from static locals to frame struct auto& aliases, eliminated bg.empty() sentinel, routed bg through PDA, added pda.replace() for Switch paths, removed file-scope bg string

## Decisions Made
- Moved `Theme::updateThemes()` from standalone `bg.empty()` sentinel into optionsMenu's redraw block (guarded by `pda.bg().empty()`) to preserve first-entry theme refresh without maintaining the sentinel pattern
- Kept `menuMask.set(SignalReturn)` in signalChoose and signalSend as-is (no PDA push) -- process() handles PDA catch-up via lazy seeding when it detects PDA empty
- Removed file-scope `string bg` entirely since all 8 functions and process() now use PDA bg lifecycle exclusively

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- FRAME-01 requirement fully delivered: all menu functions use per-frame state from the PDA stack
- Menu state is now testable and restartable -- no hidden persistent state in function-static locals
- Ready for Phase 22 (PDA dispatch) to replace menuFunc dispatch vector with std::visit on MenuFrameVar
- The auto& alias pattern means Phase 22 can change dispatch mechanism without touching function body logic

## Self-Check: PASSED

All files exist, all commits verified (20d2bda).

---
*Phase: 21-static-local-migration*
*Completed: 2026-03-02*
