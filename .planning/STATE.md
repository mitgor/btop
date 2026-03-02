---
gsd_state_version: 1.0
milestone: v1.3
milestone_name: Menu PDA + Input FSM
current_phase: 22 of 25 (PDA Dispatch)
current_plan: 0
status: ready
last_updated: "2026-03-02T11:44:21.625Z"
last_activity: 2026-03-02
progress:
  total_phases: 2
  completed_phases: 2
  total_plans: 3
  completed_plans: 3
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-02)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.
**Current focus:** Phase 21 — Static Local Migration

## Current Position

**Current Phase:** 21 of 25 (Static Local Migration)
**Current Plan:** 2
**Total Plans in Phase:** 2
**Status:** Phase complete — ready for verification
**Last Activity:** 2026-03-02

Progress: [█░░░░░░░░░] 9%

## Performance Metrics

**Velocity:**
- Total plans completed: 1
- Average duration: 3min
- Total execution time: 0.05 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 20-pda-types-skeleton | 1 | 3min | 3min |

**Recent Trend:**
- Last 5 plans: 3min
- Trend: Starting

*Updated after each plan completion*
| Phase 21 P01 | 4min | 2 tasks | 4 files |
| Phase 21 P02 | 6min | 2 tasks | 1 files |

## Accumulated Context

### Decisions

Full decision log in PROJECT.md Key Decisions table.

Recent decisions affecting current work:
- Roadmap: 6-phase dependency chain (types -> statics -> dispatch -> input FSM -> tests -> cleanup) driven by hard code dependencies
- Research: PDAAction return-value pattern must be established in Phase 20 to prevent variant self-modification UB (pitfalls 1 and 7)
- Research: Layout/interaction struct split must be established in Phase 20 to prevent stale coordinates on resize (pitfall 6)
- Research: Single-writer wrapper (menu_show/menu_close) introduced in Phase 21 as migration safety net
- 20-01: Placed all PDA types in menu namespace following btop_state.hpp pattern
- 20-01: Used std::stack with vector backing for contiguous memory
- 20-01: Assert-based preconditions on pop/replace/top rather than exceptions
- [Phase 21]: Coexistence of bg and pda.bg() during migration -- both cleared in parallel until Plan 02 migrates functions
- [Phase 21]: PDA seeded lazily on first dispatch in process() via switch on currentMenu
- [Phase 21]: menu_close_current omits invariant assert since process() may close and reopen in sequence
- [Phase 21]: Moved Theme::updateThemes() into optionsMenu redraw block guarded by pda.bg().empty() instead of standalone sentinel
- [Phase 21]: Removed file-scope string bg entirely -- PDA owns bg lifecycle exclusively after all functions migrated

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-02
Stopped at: Completed 21-01-PLAN.md (PDA Infrastructure)
Resume file: None
