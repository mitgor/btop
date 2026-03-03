---
gsd_state_version: 1.0
milestone: v1.6
milestone_name: Unified Redraw
status: ready_to_plan
last_updated: "2026-03-03"
progress:
  total_phases: 4
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-03)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.
**Current focus:** Phase 31 — DirtyFlags Foundation

## Current Position

Phase: 31 of 34 (DirtyFlags Foundation)
Plan: — (phase not yet planned)
Status: Ready to plan
Last activity: 2026-03-03 — Roadmap created for v1.6

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 0 (v1.6)

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
- Single DirtyBit::Gpu for all GPU panels (not per-panel) — matches existing force_redraw behavior
- Menu::redraw stays separate (main-thread-only, out of DirtyFlags scope)
- Conservative approach: keep draw function signatures, pass dirty via parameter
- std::atomic<uint32_t> with fetch_or/exchange; release/acquire memory ordering
- Phase 30 (from v1.5) descoped; v1.6 starts at Phase 31

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-03
Stopped at: Roadmap created for v1.6 Unified Redraw
Resume file: None
