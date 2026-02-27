---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: unknown
last_updated: "2026-02-27T20:17:37.487Z"
progress:
  total_phases: 8
  completed_phases: 8
  total_plans: 19
  completed_plans: 19
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-27)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption (CPU, RAM, startup latency, render time) while preserving every aspect of the user experience.
**Current focus:** Phase 6: Compiler & Verification -- Complete (2/2 plans)

## Current Position

Phase: 6 (Compiler & Verification)
Plan: 2 of 2 in current phase (COMPLETE)
Status: Sanitizer sweeps clean, PGO 1.1% gain measured, mimalloc 1.6% gain documented
Last activity: 2026-02-27 -- Sanitizer sweeps + PGO build + mimalloc evaluation + 06-RESULTS.md

Progress: [##########] 100% -- All 6 phases complete (18/18 plans)

## Performance Metrics

**Velocity:**
- Total plans completed: 17
- Average duration: ~8 min
- Total execution time: ~2.2 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. Profiling & Baseline | 3 | ~30 min | ~10 min |
| 2. String Allocation Reduction | 2 | 9 min | ~5 min |
| 3. I/O & Data Collection | 2 | 9 min | ~5 min |
| 3.1 Profiling Gap Closure | 1 | 5 min | 5 min |
| 4. Data Structure Modernization | 5/5 | ~70 min | ~14 min |
| 7. Benchmark Integration Fixes | 1/1 | ~3 min | ~3 min |

| 5. Rendering Pipeline | 3/3 | 19 min | ~6 min |

**Recent Trend:**
- Last 5 plans: 05-01 (7m), 05-02 (7m), 05-03 (5m), 06-01 (3m)
- Trend: 06-02 Sanitizer sweeps + PGO + mimalloc (1 task, 4 files)

*Updated after each plan completion*
| Phase 05 P01 | 7min | 2 tasks | 5 files |
| Phase 05 P02 | 7min | 2 tasks | 5 files |
| Phase 05 P03 | 5min | 2 tasks | 5 files |
| Phase 06 P01 | 3min | 2 tasks | 3 files |
| Phase 06 P02 | 8min | 1 task | 4 files |

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
- [Phase 05]: Used last_data_back single-value cache instead of per-column caching -- 80% benefit with 20% complexity
- [Phase 05]: Pre-allocate with width*height*8 reserve estimate covering escape codes + UTF-8 + cursor moves
- [Phase 05]: Incremental strategy (a): escape-string parser for differential rendering rather than direct cell writes
- [Phase 05]: Overlay/clock bypass screen buffer as ephemeral partial updates -- next full frame re-syncs
- [Phase 05]: Cell color encoding: 0=default, bit24=truecolor(RGB in low 24), else 256-color index+1
- [Phase 05]: full_emit skips default (empty) cells since terminal already has blank space from clearing
- [Phase 06]: PGO and sanitizer options mutually exclusive with FATAL_ERROR enforcement
- [Phase 06]: Sanitizer CI uses Clang 19 with libc++ for consistent toolchain
- [Phase 06]: Benchmark uses 10 cycles under sanitizer (vs 50 normally) due to 2-15x overhead
- [Phase 06]: fail-fast: false in sanitizer matrix so both asan-ubsan and tsan always run
- [Phase 06]: 3 pre-existing ASan/UBSan issues fixed inline (smc.cpp overflow, misaligned access, NaN conversion)
- [Phase 06]: PGO ~1% improvement valid result for I/O-bound workload (68% system time)
- [Phase 06]: mimalloc ~2% improvement confirms Phase 2-5 allocation reduction was effective
- [Phase 06]: pgo-build.sh uses xcrun llvm-profdata on macOS, BUILD_TESTING=OFF

### Pending Todos

None yet.

### Blockers/Concerns

- Research flagged Phase 3 (macOS/FreeBSD I/O equivalents) and Phase 5 (differential output design) as needing deeper research during planning

## Session Continuity

Last session: 2026-02-27
Stopped at: Completed 06-02-PLAN.md (sanitizer sweeps + PGO + mimalloc) -- Phase 6: 2/2 plans COMPLETE
Resume file: None
