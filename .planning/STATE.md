---
gsd_state_version: 1.0
milestone: v1.4
milestone_name: Render & Collect Modernization
current_phase: 28
current_plan: 1
status: ready
last_updated: "2026-03-02"
last_activity: 2026-03-02
progress:
  total_phases: 6
  completed_phases: 3
  total_plans: 12
  completed_plans: 5
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-02)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.
**Current focus:** Phase 28 - POSIX I/O (next after Phase 27 completion)

## Current Position

Phase: 28 of 30 (POSIX I/O) -- Phase 27 complete
Plan: 0 of ? complete in current phase
Status: Ready
Last activity: 2026-03-02 -- Phase 27 Plan 02 executed (BSD platform cpu_old enum arrays)

Progress: [####░░░░░░] 42%

## Performance Metrics

**Velocity:**
- Total plans completed: 5 (v1.4)
- Average duration: ~7min
- Total execution time: ~35min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 25-stale-static-const-fix | 1 | 1min | 1min |
| 26-themekey-enum-arrays | 2 | ~30min | ~15min |
| 27-cpu-old-enum-arrays | 2 | 5min | ~2.5min |

**Recent Trend:**
- Last 3 plans: ~12min avg
- Trend: accelerating

*Updated after each plan completion*

## Accumulated Context

### Decisions

Full decision log in PROJECT.md Key Decisions table.

Recent decisions affecting current work:
- v1.4 scope: render/collect modernization (theme enum arrays, cpu_old conversion, POSIX I/O, draw decomposition, redraw consolidation, stale static const fix)
- No new state machines in v1.4 -- bringing existing layers up to the standard set by v1.0-v1.3
- Phases 26/27/28 can proceed in parallel after Phase 25 (independent subsystems)
- Phase 29 depends on 26/27/28 (draw decomposition should happen after perf changes to draw inputs)
- Phase 30 depends on 29 (unified redraw touches all draw modules)
- [Phase 25] Remove static from hasCpuHz and freq_range in calcSizes() to match the pattern already used in Cpu::draw()
- [Phase 26] Two-enum design: ColorKey (50 entries) for individual colors, GradientKey (11 entries) for gradient base names. Constexpr name tables for theme file parsing bridge. GradientKey::COUNT used as "no gradient" sentinel in Graph class.
- [Phase 27] CpuOldField enum with 12 entries in shared header; cpu_old_to_field constexpr mapping bridges CpuOldField to CpuField; kept unordered_map include (8+ other users)
- [Phase 27] Kept #include <unordered_map> in all BSD platform files -- each has 8+ other unordered_map users beyond cpu_old

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-02
Stopped at: Completed 27-02-PLAN.md (BSD platform cpu_old enum arrays -- Phase 27 complete)
Resume file: None
