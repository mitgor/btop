---
gsd_state_version: 1.0
milestone: v1.6
milestone_name: Unified Redraw
status: executing
stopped_at: Completed 31-02-PLAN.md (Phase 31 complete)
last_updated: "2026-03-07T20:03:39.290Z"
last_activity: 2026-03-07 — Completed Plan 31-02 (Dead code removal and force_redraw rename)
progress:
  total_phases: 4
  completed_phases: 2
  total_plans: 3
  completed_plans: 3
  percent: 100
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-03)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.
**Current focus:** Phase 32 — Runner Integration

## Current Position

Phase: 32 of 34 (Runner Integration)
Plan: 1 of 1 complete (phase done)
Status: Executing
Last activity: 2026-03-08 — Completed Plan 32-01 (Wire PendingDirty into runner thread)

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**
- Total plans completed: 3 (v1.6)

| Phase | Plan | Duration | Tasks | Files |
|-------|------|----------|-------|-------|
| 31 | 01 | 2min | 1 | 3 |
| 31 | 02 | 4min | 2 | 3 |
| 32 | 01 | 4min | 2 | 4 |

**Cumulative (v1.0-v1.5):**
- v1.0: 9 phases, 20 plans
- v1.1: 6 phases, 13 plans
- v1.2: 4 phases, 4 plans
- v1.3: 5 phases, 9 plans
- v1.4: 3 phases, 5 plans
- v1.5: 2 phases, 4 plans

## Accumulated Context

### Decisions

Full decision log in PROJECT.md Key Decisions table.

Recent decisions affecting current work:
- Single-box input redraws mark only their specific DirtyBit, preserving differential emit
- ForceFullEmit exclusively drives screen_buffer.set_force_full(), separated from per-box draw dirty
- Single DirtyBit::Gpu for all GPU panels (not per-panel) — matches existing force_redraw behavior
- Menu::redraw stays separate (main-thread-only, out of DirtyFlags scope)
- Conservative approach: keep draw function signatures, pass dirty via parameter
- std::atomic<uint32_t> with fetch_or/exchange; release/acquire memory ordering
- Phase 30 (from v1.5) descoped; v1.6 starts at Phase 31
- Simplified calcSizes guard to != AppStateTag::Resizing since Proc::resized was dead code

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-08
Stopped at: Completed 32-01-PLAN.md (Phase 32 complete)
Resume file: None
