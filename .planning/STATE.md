---
gsd_state_version: 1.0
milestone: v1.7
milestone_name: Advanced Performance
status: in-progress
stopped_at: Completed 36-03-PLAN.md
last_updated: "2026-03-14T09:32:49Z"
last_activity: 2026-03-14 — Completed 36-03 (SoA key extraction for cache-friendly sorting)
progress:
  total_phases: 5
  completed_phases: 2
  total_plans: 3
  completed_plans: 3
  percent: 100
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-13)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.
**Current focus:** v1.7 Advanced Performance — Phase 36 (Algorithmic Improvements)

## Current Position

Phase: 36 (Algorithmic Improvements) — second of 5 in v1.7 — COMPLETE
Plan: 3 of 3
Status: Phase 36 complete — all 3 plans finished
Last activity: 2026-03-14 — Completed 36-03 (SoA key extraction for cache-friendly sorting)

Progress: [██████████] 100%

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
- [36-03] Used double as unified key type for SoA extraction (integers cast to double for uniform comparison)
- [36-03] thread_local vectors for SortEntry and sorted_front avoid re-allocation each cycle
- [36-03] SOA_THRESHOLD=128 validated by benchmark showing SoA wins even at N=64

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-14T09:32:49Z
Stopped at: Completed 36-03-PLAN.md (Phase 36 complete)
Resume file: None
