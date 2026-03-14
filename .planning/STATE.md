---
gsd_state_version: 1.0
milestone: v1.7
milestone_name: Advanced Performance
status: in-progress
stopped_at: Completed 39-01-PLAN.md
last_updated: "2026-03-14T14:02:03Z"
last_activity: 2026-03-14 — Completed 39-01 (SMC/IOKit handle caching)
progress:
  total_phases: 5
  completed_phases: 4
  total_plans: 10
  completed_plans: 10
  percent: 100
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-13)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.
**Current focus:** v1.7 Advanced Performance — Phase 39 (Platform I/O)

## Current Position

Phase: 39 (Platform I/O) — fifth of 5 in v1.7
Plan: 1 of 2
Status: 39-01 complete — SMC/IOKit handle caching
Last activity: 2026-03-14 — Completed 39-01 (SMC/IOKit handle caching)

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
- [Phase 37]: mimalloc NOT adopted as default (1.6% gain below 3% threshold); BTOP_MIMALLOC opt-in CMake option provided
- [37-01] Used new_delete_resource() upstream fallback for arena safety during development
- [37-01] Placed cycle_arena.release() after safety checks, before COLLECTING STATE marker
- [37-01] 64KB arena buffer with alignas(64) for cache-line alignment
- [37-02] Used std::array<long long, 10> for CPU stat times instead of pmr::vector (compile-time sized)
- [37-02] Created local read_sysfs_* helper lambdas in update_battery() instead of file-scope functions
- [37-02] Removed try/catch around battery numeric reads -- from_chars returns error code, cleaner flow
- [38-01] Used string& out parameter (not return value) to eliminate per-draw-function allocation
- [38-01] Pre-reserve 256KB in runner loop, clear each cycle -- capacity preserved across frames
- [38-01] Left Graph::operator() internal buffering unchanged per research recommendation
- [38-01] Net::draw out = box changed to out += box for correct append semantics with shared buffer
- [38-02] Used iovec[3] inline for hot-path runner frame output for clarity
- [38-02] Used write_synced lambda for overlay and clock paths to reduce repetition
- [38-02] Partial write handling advances iovec array rather than falling back to write()
- [39-01] SMCConnection singleton via Meyers static local — thread-safe lazy init guaranteed by C++11
- [39-01] Stale SMC connection detection checks kIOReturnNotReady/Aborted/NotResponding, reconnects once
- [39-01] getGpuTemp() added as public method on SMCConnection to keep SMCReadKey private
- [39-01] IOHIDEventSystemClient cached as static in getSensors() — never released, CopyServices still per-cycle

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-14T14:02:03Z
Stopped at: Completed 39-01-PLAN.md
Resume file: None
