---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: unknown
last_updated: "2026-02-27T17:01:22Z"
progress:
  total_phases: 6
  completed_phases: 5
  total_plans: 13
  completed_plans: 12
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-27)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption (CPU, RAM, startup latency, render time) while preserving every aspect of the user experience.
**Current focus:** Phase 4: Data Structure Modernization

## Current Position

Phase: 4 (Data Structure Modernization) -- Gap Closure
Plan: 4 of 5 in current phase -- COMPLETE
Status: Gap closure plan 04 complete -- BSD ring buffer fixes and benchmark migration done
Last activity: 2026-02-27 -- BSD resize-before-push fixes, bench_draw RingBuffer migration

Progress: [#########-] 92%

## Performance Metrics

**Velocity:**
- Total plans completed: 12
- Average duration: ~10 min
- Total execution time: ~1.8 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. Profiling & Baseline | 3 | ~30 min | ~10 min |
| 2. String Allocation Reduction | 2 | 9 min | ~5 min |
| 3. I/O & Data Collection | 2 | 9 min | ~5 min |
| 3.1 Profiling Gap Closure | 1 | 5 min | 5 min |
| 4. Data Structure Modernization | 4/5 | ~67 min | ~17 min |

**Recent Trend:**
- Last 5 plans: 03.1-01 (5m), 04-01 (6m), 04-02 (~45m), 04-03 (11m), 04-04 (5m)
- Trend: 04-04 fast gap closure (targeted fixes in 6 files)

*Updated after each plan completion*
| Phase 04 P04 | 5min | 2 tasks | 6 files |

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
- [Phase 04]: Kept cpu_old as local string-keyed map (scratch storage, not hot-path data structure)
- [Phase 04]: Used capacity-based resize() instead of pop_front trimming for ring buffer size management
- [Phase 04]: Migrated Net::graph_max/max_count from string-keyed maps to std::array for consistency
- [Phase 04]: Used bsd_time_fields (4 entries) vs linux_time_fields (10 entries) for platform CPU field counts
- [Phase 04]: Kept string-accepting Config overloads for menu runtime key resolution alongside enum overloads
- [Phase 04]: Added is_bool_key/is_int_key/is_string_key helpers to replace map .contains() in menu dispatch
- [Phase 04]: Used IIFE for std::array initialization with non-zero default values
- [Phase 04]: Kept string-accepting Config overloads for menu runtime key resolution alongside enum overloads
- [Phase 04]: Removed inline keyword from Config::set enum overloads to fix static library symbol export for benchmarks

### Pending Todos

None yet.

### Blockers/Concerns

- Research flagged Phase 3 (macOS/FreeBSD I/O equivalents) and Phase 5 (differential output design) as needing deeper research during planning

## Session Continuity

Last session: 2026-02-27
Stopped at: Completed 04-04-PLAN.md (BSD Ring Buffer Fixes and Benchmark Migration) -- Phase 4 plan 4/5
Resume file: None
