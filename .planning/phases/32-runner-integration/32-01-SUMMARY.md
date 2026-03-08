---
phase: 32-runner-integration
plan: 01
subsystem: rendering
tags: [dirty-flags, atomic, runner-thread, redraw]

requires:
  - phase: 31-dirty-flags-foundation
    provides: DirtyBit enum, PendingDirty struct, DirtyAll constant

provides:
  - PendingDirty wired into runner thread as sole redraw state source
  - mark_dirty() API for external call sites
  - Per-box dirty decomposition in thread loop
  - ForceFullEmit separated from per-box content redraw

affects: [33-draw-dirty-guard, 34-dirty-emit-optimization]

tech-stack:
  added: []
  patterns: [mark_dirty-before-run pattern for external call sites, single take() per draw cycle]

key-files:
  created: []
  modified:
    - src/btop.cpp
    - src/btop_shared.hpp
    - src/btop_input.cpp
    - src/btop_menu.cpp

key-decisions:
  - "Single-box input redraws mark only their specific DirtyBit, not ForceFullEmit, preserving differential emit"
  - "ForceFullEmit exclusively drives screen_buffer.set_force_full(), fully separated from per-box draw dirty"

patterns-established:
  - "mark_dirty-before-run: callers mark dirty bits then call run() with 2-param signature"
  - "Single take() per cycle: thread loop calls pending_dirty.take() once, decomposes into per-box bools"

requirements-completed: [WIRE-01, WIRE-02, WIRE-03, WIRE-04]

duration: 4min
completed: 2026-03-08
---

# Phase 32 Plan 01: Runner Integration Summary

**PendingDirty replaces pending_redraw as sole redraw state, with per-box dirty decomposition and ForceFullEmit separation in runner thread**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-08T07:44:58Z
- **Completed:** 2026-03-08T07:49:09Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Replaced atomic<bool> pending_redraw with PendingDirty instance providing per-box granularity
- Runner thread decomposes dirty bits into 5 per-box bools + force_full via single take() call
- All 13 Runner::run() call sites migrated to 2-parameter signature with mark_dirty() calls
- Single-box input redraws preserve differential emit (no ForceFullEmit)
- All 336 tests pass clean

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace pending_redraw with PendingDirty and modify runner internals** - `4dbe591` (feat)
2. **Task 2: Migrate external call sites and verify build** - `dddfdb8` (feat)

## Files Created/Modified
- `src/btop.cpp` - PendingDirty instance, mark_dirty(), dirty decomposition in thread loop, run() 2-param signature
- `src/btop_shared.hpp` - btop_dirty.hpp include, mark_dirty() declaration, run() signature updated
- `src/btop_input.cpp` - 6 call sites migrated to mark_dirty + run()
- `src/btop_menu.cpp` - 3 call sites migrated to mark_dirty + run()

## Decisions Made
- Single-box input redraws mark only their specific DirtyBit (Proc, Cpu, Mem, Net) without ForceFullEmit, preserving differential emit for key presses
- ForceFullEmit exclusively controls screen_buffer.set_force_full(), fully separated from per-box draw dirty bools

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- PendingDirty fully wired into runner thread
- Per-box dirty bools available for Phase 33 draw-dirty-guard to gate draw calls
- ForceFullEmit path ready for Phase 34 emit optimization

---
*Phase: 32-runner-integration*
*Completed: 2026-03-08*
