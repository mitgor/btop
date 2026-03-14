---
gsd_state_version: 1.0
milestone: v1.7
milestone_name: Advanced Performance
status: in-progress
stopped_at: Completed 36-02-PLAN.md
last_updated: "2026-03-14T09:25:00Z"
last_activity: 2026-03-14 — Completed 36-02 (constexpr escape sequences and lookup tables)
progress:
  total_phases: 5
  completed_phases: 1
  total_plans: 3
  completed_plans: 2
  percent: 67
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-13)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.
**Current focus:** v1.7 Advanced Performance — Phase 36 (Algorithmic Improvements)

## Current Position

Phase: 36 (Algorithmic Improvements) — second of 5 in v1.7
Plan: 2 of 3
Status: Plan 36-02 complete
Last activity: 2026-03-14 — Completed 36-02 (constexpr escape sequences and lookup tables)

Progress: [██████----] 67%

## Performance Metrics

**Cumulative (v1.0-v1.6):**
- v1.0: 9 phases, 20 plans
- v1.1: 6 phases, 13 plans
- v1.2: 4 phases, 4 plans
- v1.3: 5 phases, 9 plans
- v1.4: 3 phases, 5 plans
- v1.5: 2 phases, 4 plans
- v1.6: 4 phases, 5 plans

## Accumulated Context

### Decisions

Full decision log in PROJECT.md Key Decisions table.

- [35-01] 7-phase PGO training workload exercising sort keys, filtering, tree mode, resize, box configs, draw-only
- [35-01] Moved Clang detection before Step 2 in pgo-build.sh for LLVM_PROFILE_FILE support
- [35-02] PCH includes only STL/fmt headers (never project headers) to avoid invalidation during development
- [35-02] CMake options declared early, applied late after targets and includes exist
- [36-01] Used local proc_info mirror in benchmark to avoid libbtop compilation dependency
- [36-01] partial_sort is not stable but acceptable - numeric keys rarely tie, list changes every cycle
- [36-02] Used inline constexpr const char* for Fx/Mv/Term instead of string_view to preserve string+ operator compatibility
- [36-02] Used linear-scan for Key_escapes lookup instead of binary search (34 entries at human input rate)
- [36-02] Kept valid_boxes as runtime vector due to conditional #ifdef GPU_SUPPORT entries
- [36-02] Created static vector copies in menu for constexpr array compatibility with optionsList

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-14T09:25:00Z
Stopped at: Completed 36-02-PLAN.md
Resume file: None
