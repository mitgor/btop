---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: unknown
last_updated: "2026-02-27T18:09:21.287Z"
progress:
  total_phases: 6
  completed_phases: 6
  total_plans: 14
  completed_plans: 14
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-27)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption (CPU, RAM, startup latency, render time) while preserving every aspect of the user experience.
**Current focus:** Phase 5: Rendering Pipeline -- Plan 2 of 3 complete

## Current Position

Phase: 5 (Rendering Pipeline)
Plan: 2 of 3 in current phase -- COMPLETE
Status: Graph last_data_back caching optimization implemented with unit tests and benchmarks
Last activity: 2026-02-27 -- btop_draw.hpp/cpp graph cache + test_graph_cache.cpp + bench_draw.cpp

Progress: [##########] 100% (prior phases) + Phase 5: 2/3 plans

## Performance Metrics

**Velocity:**
- Total plans completed: 16
- Average duration: ~9 min
- Total execution time: ~2.1 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. Profiling & Baseline | 3 | ~30 min | ~10 min |
| 2. String Allocation Reduction | 2 | 9 min | ~5 min |
| 3. I/O & Data Collection | 2 | 9 min | ~5 min |
| 3.1 Profiling Gap Closure | 1 | 5 min | 5 min |
| 4. Data Structure Modernization | 5/5 | ~70 min | ~14 min |
| 7. Benchmark Integration Fixes | 1/1 | ~3 min | ~3 min |

| 5. Rendering Pipeline | 1/3 | 7 min | ~7 min |

**Recent Trend:**
- Last 5 plans: 04-04 (5m), 04-05 (3m), 07-01 (3m), 05-01 (7m)
- Trend: 05-01 rendering pipeline first plan (2 tasks, 5 files)

*Updated after each plan completion*
| Phase 05 P01 | 7min | 2 tasks | 5 files |

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
- [Phase 04]: RingBuffer single-op throughput comparable to deque; key benefit is zero heap allocations in steady state
- [Phase 04]: SC1/SC2 formally descoped for macOS (Cachegrind/heaptrack Linux-only); architectural rationale + Linux CI instructions provided
- [Phase 04]: Benchmark kept self-contained without btop runtime init, using local data structures replicating old patterns
- [Phase 07]: Used if-else init pattern instead of try-catch-bench to separate init failure from benchmark execution
- [Phase 07]: Emit SKIP to stdout (not just stderr) for CI log visibility
- [Phase 07]: Write empty JSON results on proc crash to avoid downstream JSONDecodeError
- [Phase 05]: Moved unistd.h include out of __linux__ guard for cross-platform STDOUT_FILENO access
- [Phase 05]: Used thread_local string for overlay buffer in main runner to avoid per-frame heap allocation
- [Phase 05]: Used concurrent reader thread in large write test to avoid pipe buffer deadlock on macOS

### Pending Todos

None yet.

### Blockers/Concerns

- Research flagged Phase 3 (macOS/FreeBSD I/O equivalents) and Phase 5 (differential output design) as needing deeper research during planning

## Session Continuity

Last session: 2026-02-27
Stopped at: Completed 05-01-PLAN.md (POSIX write_stdout) -- Phase 5: 1/3 plans complete
Resume file: None
