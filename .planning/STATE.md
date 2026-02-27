---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: in-progress
last_updated: "2026-02-27T15:46:40Z"
progress:
  total_phases: 5
  completed_phases: 4
  total_plans: 11
  completed_plans: 9
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-27)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption (CPU, RAM, startup latency, render time) while preserving every aspect of the user experience.
**Current focus:** Phase 4: Data Structure Modernization

## Current Position

Phase: 4 (Data Structure Modernization)
Plan: 1 of 3 in current phase -- COMPLETE
Status: Plan 04-01 complete -- foundational types created
Last activity: 2026-02-27 -- RingBuffer template, field enums, graph_symbols/graphs migration

Progress: [########--] 82%

## Performance Metrics

**Velocity:**
- Total plans completed: 9
- Average duration: ~6 min
- Total execution time: ~1.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. Profiling & Baseline | 3 | ~30 min | ~10 min |
| 2. String Allocation Reduction | 2 | 9 min | ~5 min |
| 3. I/O & Data Collection | 2 | 9 min | ~5 min |
| 3.1 Profiling Gap Closure | 1 | 5 min | 5 min |
| 4. Data Structure Modernization | 1/3 | 6 min | 6 min |

**Recent Trend:**
- Last 5 plans: 02-02 (3m), 03-01 (4m), 03-02 (5m), 03.1-01 (5m), 04-01 (6m)
- Trend: Stable ~5-6 min

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Roadmap: Profile-first approach -- no optimization code until baseline measurements exist
- Roadmap: Phases 2 and 3 are independent (string vs I/O) but sequenced for clean attribution
- Roadmap: Data structure changes (Phase 4) deferred until collectors and string utils are stable
- Roadmap: PGO applied last (Phase 6) so compiler profiles reflect optimized code
- [Phase 02]: Removed <regex> include from btop_tools.hpp; no remaining regex usage in header
- [Phase 02]: Used forward-copy O(n) parser for uncolor instead of erase-in-loop O(n^2) that caused musl issues
- [Phase 02]: Replaced str.resize() with str.substr() returns for const-ref correctness in string utils
- [Phase 02]: Retained 2 fmt::format calls in non-hot paths (TextEdit, battery watts) -- negligible gain vs complexity
- [Phase 02]: Used per-function reserve multipliers (10-18x) based on escape density rather than uniform multiplier
- [Phase 03]: Used rfind(')') for /proc/[pid]/stat comm field skipping instead of offset-based space counting
- [Phase 03]: Replaced ifstream line-by-line status parsing with string_view::find for Uid field extraction
- [Phase 03]: Removed unused long_string and short_str from PID loop scope after POSIX conversion
- [Phase 03]: proc_map owns process data; current_procs vector rebuilt from map each cycle for sorting/return compatibility
- [Phase 03]: alive_pids declared static to persist across collection cycles for tree view ppid orphan detection
- [Phase 03]: FreeBSD idle process skip adapted to erase from proc_map and alive_pids instead of pop_back
- [Phase 03]: Linux kernel thread filtering uses alive_pids.erase(pid) instead of found.pop_back()
- [Phase 03.1]: Used macOS `sample` command for CPU profiling instead of Instruments/xctrace (simpler automation)
- [Phase 03.1]: 500 benchmark cycles for sufficient sampling data; both flat and inclusive profiles documented
- [Phase 04]: GraphSymbolType enum and helpers placed in Draw namespace (btop_draw.hpp) for test accessibility
- [Phase 04]: graph_bg_symbol() helper eliminates repeated default-resolution + index-6 lookup in 4 draw functions
- [Phase 04]: shared_gpu_percent migration deferred to Plan 02 -- requires simultaneous access-site updates

### Pending Todos

None yet.

### Blockers/Concerns

- Research flagged Phase 3 (macOS/FreeBSD I/O equivalents) and Phase 5 (differential output design) as needing deeper research during planning

## Session Continuity

Last session: 2026-02-27
Stopped at: Completed 04-01-PLAN.md (Foundational Types) -- Phase 4 plan 1 of 3 complete
Resume file: None
