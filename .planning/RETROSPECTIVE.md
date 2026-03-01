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

## Milestone: v1.1 — Automata Architecture

**Shipped:** 2026-03-01
**Phases:** 6 | **Plans:** 13 | **Sessions:** ~4

### What Was Built
- App FSM: AppStateTag enum replacing 7 atomic<bool> flags, AppStateVar std::variant with per-state data (Running, Resizing, Sleeping, Quitting, Error)
- Event system: Lock-free SPSC EventQueue with async-signal-safe push, AppEvent std::variant (TimerTick, KeyInput, Resize, Quit, Sleep, Resume, Reload, ThreadError)
- Dispatch infrastructure: on_event() overloads with std::visit, dispatch_event(), transition_to() with entry/exit actions
- Runner FSM: RunnerStateTag enum (Idle, Collecting, Drawing, Stopping), RunnerStateVar variant, typed wrappers replacing atomic bool flags
- Bug fixes: Ctrl+C hang (clean_quit re-entrancy guard), resume-no-redraw (on_event(Sleeping, Resume))
- Verification: 279 tests across 3 sanitizer configs (ASan+UBSan+TSan), zero findings, human-approved behavior preservation

### What Worked
- **Incremental migration**: Each of 6 phases shipped independently with no regressions — enum → events → dispatch → variant → runner → verify
- **Shadow atomic pattern**: Maintaining AppStateTag atomic alongside AppStateVar variant allowed gradual migration without breaking cross-thread reads
- **Test-first in verification phase**: Writing 40 targeted FSM tests in Phase 15 caught no regressions, validating the migration quality
- **Phased research**: AUTOMATA-ARCHITECTURE.md research doc set clear architectural direction before any code was written
- **Rapid velocity**: 13 plans in ~50 minutes total execution time (avg 3.8 min/plan) — well-scoped plans with clear success criteria

### What Was Inefficient
- **runner_var write-only**: RunnerStateVar was created but never read at runtime — shadow atomic alone drives all behavior. The variant infrastructure exists for future use but is dead code today
- **ThreadError dead code path**: on_event(Running, ThreadError) is unreachable because runner writes shadow atomic directly instead of pushing events
- **Shadow atomic proliferation**: The dual-state pattern (variant + shadow atomic) creates consistency opportunities that required careful audit to catalog

### Patterns Established
- **std::variant + std::visit** as the FSM pattern for btop (compile-time exhaustiveness, zero dependencies)
- **Shadow atomic for cross-thread state** — main-thread variant + atomic tag for cross-thread reads
- **Lock-free SPSC queue** for signal handler → main loop communication (cache-line aligned, power-of-2 capacity)
- **on_event overloads** as pure transition functions (return new state, no side effects) + on_enter/on_exit for effects
- **transition_to()** as single transition point with automatic entry/exit action dispatch

### Key Lessons
1. **Shadow atomics create consistency debt** — the dual-state pattern works but requires explicit documentation of which paths use which representation
2. **Dead code is acceptable during phased migration** — ThreadError handler and runner_var are scaffolding for future purity improvements
3. **Type-safe FSMs with std::variant are practical** — 63 files modified with zero behavior regressions proves the approach scales
4. **Sanitizer sweeps validate architectural changes** — zero ASan/UBSan/TSan findings across 279 tests confirms thread-safety of the new architecture
5. **Event queue decoupling simplifies signal handling** — signal handlers reduced to trivial push + kill, all logic in main loop

### Cost Observations
- Model mix: ~80% opus, ~15% sonnet, ~5% haiku (research agents)
- Sessions: ~4 (research, planning, execution, verification + audit)
- Notable: Fastest milestone by plan execution time (3.8 min avg vs 8 min avg in v1.0)

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Sessions | Phases | Key Change |
|-----------|----------|--------|------------|
| v1.0 | ~6 | 9 | Established profile-first + audit-driven gap closure |
| v1.1 | ~4 | 6 | Incremental FSM migration with shadow atomics, fastest plan velocity (3.8 min avg) |

### Cumulative Quality

| Milestone | Tests | Sanitizer Status | Dependencies Added |
|-----------|-------|-----------------|-------------------|
| v1.0 | 239 (benchmark + unit) | ASan/UBSan/TSan clean | nanobench (header-only, test-only) |
| v1.1 | 279 (unit + FSM) | ASan/UBSan/TSan clean | None |

### Top Lessons (Verified Across Milestones)

1. Profile before optimizing — confirmed by v1.0 (nanobench + macOS sampling guided all 20 plans)
2. Audit at milestone boundaries — confirmed by v1.0 (caught 2 integration gaps), v1.1 (cataloged 3 tech debt items)
3. Incremental migration over big-bang — confirmed by v1.1 (6 phases, each independently deployable, zero regressions)
4. Sanitizer sweeps as safety net — confirmed by v1.0 + v1.1 (zero findings across all optimizations and architectural changes)
