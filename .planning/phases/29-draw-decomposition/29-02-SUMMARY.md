---
phase: 29-draw-decomposition
plan: 02
subsystem: rendering
tags: [draw, decomposition, cpu, battery, refactor]

# Dependency graph
requires:
  - phase: 29-draw-decomposition plan 01
    provides: Proc::draw() decomposition pattern
provides:
  - Decomposed Cpu::draw() with 5 named sub-functions
  - Battery state tracking fully encapsulated in draw_cpu_battery()
affects: [30-redraw-consolidation]

# Tech tracking
tech-stack:
  added: []
  patterns: [namespace-scoped sub-functions for draw decomposition, return-cy pattern for cross-function row tracking]

key-files:
  created: []
  modified: [src/btop_draw.cpp]

key-decisions:
  - "bat_pos/bat_len moved from static locals to Cpu namespace scope for cross-function access"
  - "draw_cpu_cores returns cy (int) for GPU info row positioning instead of shared mutable state"
  - "Removed unused parameters (graph_symbol, tty_mode, graph_bg, temp_scale, cpu_bottom) from sub-function signatures to avoid -Wunused-parameter warnings"

patterns-established:
  - "Cpu::draw decomposition: 5 sub-functions (redraw, battery, graphs, cores, gpu_info) as namespace-scoped helpers"
  - "Return value for cross-section state: draw_cpu_cores returns cy for GPU positioning"

requirements-completed: [DRAW-02, DRAW-03]

# Metrics
duration: 32min
completed: 2026-03-02
---

# Phase 29 Plan 02: Cpu::draw() Decomposition Summary

**Decomposed 454-line Cpu::draw() into 5 namespace-scoped sub-functions (redraw, battery, graphs, cores, gpu_info) with 93-line orchestrator and battery state fully encapsulated**

## Performance

- **Duration:** 32 min
- **Started:** 2026-03-02T22:03:20Z
- **Completed:** 2026-03-02T22:35:30Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Cpu::draw() reduced from 454 lines to 93-line orchestrator calling 5 named sub-functions
- Battery state tracking (old_percent, old_seconds, old_watts, old_status, bat_meter, bat_symbols) fully encapsulated in draw_cpu_battery()
- bat_pos/bat_len elevated from static locals to namespace scope for clean cross-function access
- All #ifdef GPU_SUPPORT and #ifdef __linux__ guards preserved exactly
- Zero warnings, all 330 tests passing, pure structural refactor with no logic changes

## Task Commits

Each task was committed atomically:

1. **Task 1: Extract Cpu::draw() sub-functions and reduce top-level to under 100 lines** - `f749284` (feat)
2. **Task 2: Verify Cpu::draw() byte-identical output and cross-platform build** - verification only, no code changes

## Files Created/Modified
- `src/btop_draw.cpp` - Decomposed Cpu::draw() into draw_cpu_redraw, draw_cpu_battery, draw_cpu_graphs, draw_cpu_cores, draw_cpu_gpu_info

## Decisions Made
- Moved bat_pos/bat_len to namespace scope instead of keeping as static locals -- needed for both draw_cpu_battery() and the else-cleanup branch
- Made draw_cpu_cores() return cy (int) instead of using a reference parameter -- cleaner flow for GPU info positioning
- Removed unused parameters from sub-function signatures (graph_symbol, tty_mode, graph_bg, temp_scale, cpu_bottom) that were originally captured by lambdas but not needed by the extracted functions -- avoids -Wunused-parameter warnings

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed unused parameters from sub-function signatures**
- **Found during:** Task 1
- **Issue:** Parameters like graph_symbol, tty_mode, graph_bg, temp_scale were passed to sub-functions but not used in their bodies, causing -Wunused-parameter warnings
- **Fix:** Removed unused parameters from draw_cpu_redraw (graph_bg, temp_scale, tty_mode), draw_cpu_graphs (graph_symbol, tty_mode), draw_cpu_cores (graph_symbol), draw_cpu_gpu_info (graph_symbol), and draw_cpu_battery (cpu_bottom)
- **Files modified:** src/btop_draw.cpp
- **Verification:** Clean build with zero warnings
- **Committed in:** f749284 (Task 1 commit)

**2. [Rule 1 - Bug] Fixed cy flow between cores and GPU info sections**
- **Found during:** Task 1
- **Issue:** Original code shared local variable cy between core rendering and GPU info positioning; naive extraction broke this flow
- **Fix:** Made draw_cpu_cores return cy value; draw() passes it to draw_cpu_gpu_info; added (void)cy for non-GPU builds
- **Files modified:** src/btop_draw.cpp
- **Verification:** Build succeeds, positioning logic preserved
- **Committed in:** f749284 (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 bug fixes)
**Impact on plan:** Both fixes necessary for correctness -- unused parameter warnings and cross-function variable flow. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Both Proc::draw() (plan 01) and Cpu::draw() (plan 02) are now decomposed
- Phase 29 complete -- ready for Phase 30 (redraw consolidation)
- All draw functions have clean sub-function structure for future modification

---
*Phase: 29-draw-decomposition*
*Completed: 2026-03-02*
