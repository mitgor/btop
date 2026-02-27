# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v1.0 — Performance Optimization

**Shipped:** 2026-02-27
**Phases:** 9 | **Plans:** 20 | **Sessions:** ~6

### What Was Built
- Benchmark infrastructure: nanobench microbenchmarks, --benchmark CLI mode, CI regression detection with github-action-benchmark
- String optimization: regex-free Fx::uncolor(), const-ref string params, fmt::format_to in draw path, escape-aware reserves
- I/O optimization: POSIX read_proc_file() with stack buffers, double-stat elimination, hash-map PID lookup across 3 platforms
- Data structure modernization: RingBuffer<T> template, enum-indexed arrays replacing all unordered_map<string,T>, Config enum migration (436 call sites)
- Rendering pipeline: write_stdout() POSIX helper, graph column caching, differential cell-buffer terminal output
- Compiler tooling: PGO build pipeline, ASan/UBSan/TSan CI sweeps, mimalloc evaluation
- CI hardening: btop_bench_ds in benchmark pipeline, PGO validation workflow, silent failure elimination

### What Worked
- **Profile-first approach**: Establishing benchmarks before optimizing prevented wasted effort and gave clear attribution
- **Phase sequencing**: Strings → I/O → data structures → rendering → compiler was the right order (each layer stable before the next built on it)
- **Gap closure phases**: Phases 3.1, 7, 8 were inserted to close audit-identified gaps — this kept quality high without disrupting flow
- **Milestone audit before completion**: The 3-source cross-reference (VERIFICATION + SUMMARY + REQUIREMENTS) caught real integration gaps
- **Single-day execution**: All 20 plans executed in ~14 hours with consistent velocity (~8 min/plan average)

### What Was Inefficient
- **Phase 4 was largest (5 plans)**: Config enum migration (436 call sites) could have been scoped more tightly — but was justified by the access pattern speedup
- **PGO aspirational target (10%) vs reality (1.1%)**: The I/O-bound ceiling was predictable from profiling data but wasn't flagged earlier in planning
- **Human verification items deferred**: 11 items across phases requiring runtime measurement were noted but not resolved — acceptable tech debt but could accumulate

### Patterns Established
- **Enum-indexed arrays** as the default for fixed-key data in C++ (replacing unordered_map<string,T>)
- **RingBuffer<T>** for all time-series/history data (replacing deque with steady-state zero allocations)
- **POSIX read() with stack buffers** for /proc and similar pseudo-filesystem reads
- **write_stdout()** as single write point for all terminal output
- **Differential rendering** via cell buffer comparison between frames
- **nanobench + CI** as the standard benchmark approach for C++ projects

### Key Lessons
1. **I/O-bound workloads have a low PGO ceiling** — profiling data should inform PGO expectations early
2. **Audit-driven gap closure works** — running formal audits between phases catches integration issues that per-phase verification misses
3. **Enum migration scales well** — even 436 call sites can be migrated systematically with compiler errors as guides
4. **Cross-platform optimization is feasible** — the same patterns (hash-map PID, POSIX I/O, write batching) apply to Linux, macOS, and FreeBSD with platform-specific adapters
5. **Benchmark infrastructure pays for itself** — every subsequent optimization had immediate feedback via CI

### Cost Observations
- Model mix: ~70% opus, ~20% sonnet, ~10% haiku (research agents)
- Sessions: ~6 (planning, execution waves, audit, completion)
- Notable: Wave-based execution with parallel agents kept plan execution fast (~3-14 min each)

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Sessions | Phases | Key Change |
|-----------|----------|--------|------------|
| v1.0 | ~6 | 9 | Established profile-first + audit-driven gap closure |

### Cumulative Quality

| Milestone | Benchmarks | Sanitizer Status | Dependencies Added |
|-----------|------------|-----------------|-------------------|
| v1.0 | 5 bench binaries + CI | ASan/UBSan/TSan clean | nanobench (header-only, test-only) |

### Top Lessons (Verified Across Milestones)

1. Profile before optimizing — confirmed by v1.0 (nanobench + macOS sampling guided all 20 plans)
2. Audit at milestone boundaries — confirmed by v1.0 (caught 2 integration gaps closed by Phases 7-8)
