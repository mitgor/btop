---
phase: 33-calcsizes-decoupling
plan: 01
subsystem: draw
tags: [redraw, dirty-bits, calcsizes, decoupling]

requires:
  - phase: 32-runner-integration
    provides: PendingDirty wired into runner thread with request_redraw() and mark_dirty()
provides:
  - calcSizes() fully decoupled from per-namespace redraw bool assignment
  - all redraw signaling flows exclusively through PendingDirty mechanism
affects: [34-calcsizes-guard]

tech-stack:
  added: []
  patterns: [dirty-bit-only redraw signaling]

key-files:
  created: []
  modified: [src/btop_draw.cpp]

key-decisions:
  - "Removed single redundant line; no refactoring needed beyond deletion"

patterns-established:
  - "All redraw signaling through PendingDirty: no direct per-namespace redraw bool writes outside draw functions"

requirements-completed: [DECPL-01, DECPL-02, DECPL-03]

duration: 1min
completed: 2026-03-08
---

# Phase 33 Plan 01: calcSizes Decoupling Summary

**Removed redundant per-namespace redraw bool assignment from calcSizes(), completing exclusive PendingDirty redraw signaling**

## Performance

- **Duration:** 1 min
- **Started:** 2026-03-08T08:11:33Z
- **Completed:** 2026-03-08T08:12:17Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Removed `Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true` from calcSizes() (was line 2936)
- Verified all 336 tests pass with no regressions
- Confirmed all 6 calcSizes() call sites produce correct dirty state through PendingDirty
- Verified request_redraw() uses pending_dirty.mark(DirtyAll | ForceFullEmit)
- Confirmed Menu::redraw preserved (out of scope per requirements)

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove redundant bool assignment from calcSizes** - `eb9c954` (feat)
2. **Task 2: Build verification and call-site validation** - verification only, no code changes

## Files Created/Modified
- `src/btop_draw.cpp` - Removed redundant per-namespace redraw bool assignment line from calcSizes()

## Decisions Made
None - followed plan as specified. Single line removal as planned.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- calcSizes() fully decoupled from direct redraw bool assignment
- Ready for Phase 34 (calcSizes guard) which can now treat calcSizes as pure geometry computation
- PendingDirty is the sole redraw signaling mechanism across all paths

---
*Phase: 33-calcsizes-decoupling*
*Completed: 2026-03-08*
