---
phase: 31-dirty-flags-foundation
plan: 02
subsystem: core
tags: [refactor, dead-code-removal, naming]

# Dependency graph
requires:
  - phase: 31-dirty-flags-foundation-01
    provides: "DirtyBit enum and PendingDirty struct (TDD tests)"
provides:
  - "Clean codebase with no Proc::resized references"
  - "Unambiguous force_redraw naming in btop_input.cpp"
  - "Simplified calcSizes guard using only AppStateTag::Resizing"
affects: [32-dirty-integration, 33-redraw-batching, 34-draw-migration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "force_redraw naming for local redraw variables in input handlers"

key-files:
  created: []
  modified:
    - src/btop_shared.hpp
    - src/btop_draw.cpp
    - src/btop_input.cpp

key-decisions:
  - "Simplified calcSizes guard to != instead of not(...or...) since Proc::resized was never set true"

patterns-established:
  - "force_redraw: local variable naming convention for input handler redraw flags"

requirements-completed: [CLEAN-01, CLEAN-02, CLEAN-03]

# Metrics
duration: 4min
completed: 2026-03-07
---

# Phase 31 Plan 02: Dead Code Removal Summary

**Removed dead Proc::resized atomic and renamed ambiguous bool redraw locals to force_redraw across all input handlers**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-07T19:54:28Z
- **Completed:** 2026-03-07T19:58:37Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Removed Proc::resized declaration, definition, and read site (3 locations)
- Simplified calcSizes() guard from dual condition to single AppStateTag::Resizing check
- Renamed all 4 local bool redraw variables to force_redraw in btop_input.cpp (17 total references)

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove dead Proc::resized and simplify calcSizes guard** - `aa239f3` (refactor)
2. **Task 2: Rename bool redraw to force_redraw in btop_input.cpp** - `8d2e1e4` (refactor)

## Files Created/Modified
- `src/btop_shared.hpp` - Removed extern atomic<bool> resized from Proc namespace
- `src/btop_draw.cpp` - Removed resized definition and simplified calcSizes guard
- `src/btop_input.cpp` - Renamed all 4 local bool redraw to force_redraw

## Decisions Made
- Simplified the calcSizes guard from `not (Proc::resized or ... == Resizing)` to `!= Resizing` since Proc::resized was never set to true anywhere in the codebase, making it dead code

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Codebase is clean for Phase 32-34 migration work
- No Proc::resized collisions remain
- force_redraw naming is unambiguous and ready for dirty flag integration

---
*Phase: 31-dirty-flags-foundation*
*Completed: 2026-03-07*
