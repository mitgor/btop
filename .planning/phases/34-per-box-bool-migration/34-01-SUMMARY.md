---
phase: 34-per-box-bool-migration
plan: 01
subsystem: architecture
tags: [redraw, dirty-bits, cross-thread, migration, atomic]

# Dependency graph
requires:
  - phase: 32-runner-integration
    provides: Runner::mark_dirty() API and DirtyBit enum
  - phase: 33-calcsizes-decoupling
    provides: Decoupled calcSizes from redraw booleans
provides:
  - All collector redraw signaling via Runner::mark_dirty(DirtyBit::Xxx)
  - Removal of 5 extern redraw declarations from btop_shared.hpp
  - PendingDirty as sole cross-thread redraw mechanism
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Collector uses Runner::mark_dirty(DirtyBit::Xxx) instead of namespace bool assignment"
    - "PendingDirty is the sole cross-thread redraw signaling mechanism"

key-files:
  created: []
  modified:
    - src/btop_shared.hpp
    - src/linux/btop_collect.cpp
    - src/osx/btop_collect.cpp
    - src/freebsd/btop_collect.cpp
    - src/netbsd/btop_collect.cpp
    - src/openbsd/btop_collect.cpp

key-decisions:
  - "Namespace-scope redraw definitions in btop_draw.cpp preserved as file-local variables for draw self-invalidation"

patterns-established:
  - "Collectors signal redraw needs through Runner::mark_dirty, never through direct bool assignment"

requirements-completed: [MIGR-01, MIGR-02, MIGR-03, MIGR-04, MIGR-05, MIGR-06]

# Metrics
duration: 3min
completed: 2026-03-08
---

# Phase 34 Plan 01: Per-Box Bool Migration Summary

**Migrated all 35 collector redraw=true sites to Runner::mark_dirty(DirtyBit::Xxx) and removed 5 extern redraw declarations from btop_shared.hpp**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-08T08:32:36Z
- **Completed:** 2026-03-08T08:35:54Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Removed all 5 extern redraw declarations from btop_shared.hpp (Gpu vector<bool>, Cpu, Mem, Net, Proc)
- Replaced all 35 collector `redraw = true` sites across 5 platform collectors with `Runner::mark_dirty(DirtyBit::Xxx)`
- Preserved file-local redraw definitions in btop_draw.cpp for draw self-invalidation
- All 336 tests pass with zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove extern declarations and migrate all collector write sites** - `add62ff` (feat)
2. **Task 2: Build verification and full test suite** - verification only, no code changes

## Files Created/Modified
- `src/btop_shared.hpp` - Removed 5 extern redraw declarations (Gpu, Cpu, Mem, Net, Proc)
- `src/linux/btop_collect.cpp` - 7 sites migrated to Runner::mark_dirty
- `src/osx/btop_collect.cpp` - 7 sites migrated to Runner::mark_dirty
- `src/freebsd/btop_collect.cpp` - 7 sites migrated to Runner::mark_dirty
- `src/netbsd/btop_collect.cpp` - 7 sites migrated to Runner::mark_dirty
- `src/openbsd/btop_collect.cpp` - 7 sites migrated to Runner::mark_dirty

## Decisions Made
- Namespace-scope redraw definitions in btop_draw.cpp preserved as file-local variables (Cpu, Gpu, Mem, Net, Proc) for draw function self-invalidation

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- PendingDirty is now the sole mechanism for cross-thread redraw signaling
- All per-box namespace redraw bools are fully decoupled from collectors
- The unified redraw milestone (v1.6) is complete

---
*Phase: 34-per-box-bool-migration*
*Completed: 2026-03-08*
