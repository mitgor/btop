# Roadmap: btop Performance Optimization

## Overview

This roadmap delivers measurable performance improvements to btop++ across CPU usage, memory footprint, startup time, and rendering speed. The approach is strictly evidence-driven: establish baselines first, then attack bottlenecks in order of impact and risk, moving from safe leaf-level changes (strings, I/O) through structural changes (data structures, rendering pipeline) to compiler-level optimization last. All 20 v1 requirements are covered across 6 phases. Every optimization must be justified by profiling data and must preserve the full user experience unchanged.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Profiling & Baseline** - Establish reproducible benchmarks and identify actual bottlenecks before any code changes
- [x] **Phase 2: String & Allocation Reduction** - Eliminate per-frame string allocation overhead in utilities and draw path
- [x] **Phase 3: I/O & Data Collection** - Replace expensive ifstream/filesystem operations with POSIX I/O in collectors
- [ ] **Phase 3.1: Profiling Gap Closure** - INSERTED — Run CPU sampling profiling session and document hot function ranking
- [ ] **Phase 4: Data Structure Modernization** - Replace hash maps and deques with enum-indexed arrays and ring buffers
- [ ] **Phase 5: Rendering Pipeline** - Implement differential terminal output and batch writes for minimal redraw
- [ ] **Phase 6: Compiler & Verification** - Apply PGO, evaluate mimalloc, and verify correctness with full sanitizer sweep
- [x] **Phase 7: Benchmark Integration Fixes** - Fix stale sub-benchmarks and CI reliability to close integration/flow gaps from audit

## Phase Details

### Phase 1: Profiling & Baseline
**Goal**: Quantified baseline measurements exist for all performance dimensions, confirmed hotspot ranking guides all subsequent optimization work
**Depends on**: Nothing (first phase)
**Requirements**: PROF-01, PROF-02, PROF-03
**Success Criteria** (what must be TRUE):
  1. Running the benchmark suite produces repeatable CPU usage, memory footprint, startup time, and frame render time numbers on Linux, macOS, and FreeBSD
  2. A ranked list of hot functions (with percentage contribution) exists from perf/Instruments profiling data
  3. Micro-benchmarks for the top hot functions (Proc::collect, draw functions, Fx::uncolor, string utilities) can be run individually with nanobench and produce stable results
  4. A CI job detects performance regressions by comparing against the recorded baseline
**Plans**: 3 plans in 2 waves

Plans:
- [x] 01-01-PLAN.md -- Benchmark build infrastructure + string/draw/proc micro-benchmarks (Wave 1)
- [x] 01-02-PLAN.md -- Add --benchmark CLI mode for end-to-end measurement (Wave 1)
- [x] 01-03-PLAN.md -- CI performance regression detection workflow (Wave 2)

### Phase 2: String & Allocation Reduction
**Goal**: Per-frame heap allocation count in the draw path and string utilities is measurably reduced, with regex hot path eliminated
**Depends on**: Phase 1
**Requirements**: STRN-01, STRN-02, STRN-03, STRN-04
**Success Criteria** (what must be TRUE):
  1. Fx::uncolor() uses a hand-written ANSI escape parser instead of std::regex, and heaptrack confirms the regex allocation spike is gone
  2. String utility functions (uresize, luresize, ljust, rjust, cjust) accept const ref or string_view parameters -- no copies on call
  3. Draw functions use fmt::format_to with back_inserter instead of fmt::format, eliminating intermediate string allocations in the render path
  4. Draw functions call string::reserve() with size estimates that account for escape code overhead, and heaptrack shows reduced reallocation count per frame
**Plans**: 2 plans in 2 waves

Plans:
- [x] 02-01-PLAN.md -- Replace Fx::uncolor() regex with hand-written parser + const-ref string utility params (Wave 1)
- [x] 02-02-PLAN.md -- Convert fmt::format to fmt::format_to in draw pipeline + escape-aware reserve estimates (Wave 2)

### Phase 3: I/O & Data Collection
**Goal**: System data collection uses minimal syscalls and zero heap-allocating file I/O, with O(1) PID lookup replacing linear scan
**Depends on**: Phase 1
**Requirements**: IODC-01, IODC-02, IODC-03, IODC-04
**Success Criteria** (what must be TRUE):
  1. Linux Proc::collect() reads /proc files via POSIX read() with stack buffers -- strace confirms ifstream heap allocations are eliminated and syscall count per update is reduced by 40%+
  2. readfile() no longer calls fs::exists() before opening -- double-stat eliminated for every file read
  3. PID lookup in current_procs uses a hash-based container -- process collection time scales linearly (not quadratically) with process count
  4. macOS and FreeBSD collectors have equivalent optimizations applied to their respective APIs (sysctl, proc_pidinfo, kvm_getprocs)
**Plans**: 2 plans in 2 waves

Plans:
- [x] 03-01-PLAN.md -- POSIX read_proc_file() helper + readfile() double-stat fix + Linux ifstream replacement (Wave 1)
- [x] 03-02-PLAN.md -- Hash-map PID lookup for Linux, macOS, and FreeBSD collectors (Wave 2)

### Phase 3.1: Profiling Gap Closure
**Goal**: A ranked list of hot functions with CPU % contribution exists from an actual perf or Instruments profiling session, closing the Phase 1 SC2 gap
**Depends on**: Phase 1
**Requirements**: PROF-01
**Gap Closure**: Closes Phase 1 SC2 gap from v1.0 milestone audit
**Success Criteria** (what must be TRUE):
  1. A CPU sampling profiling session (perf record on Linux or Instruments on macOS) has been run against btop with realistic workload
  2. A PROFILING.md document exists with ranked hot functions showing CPU % attribution from the profiling data
  3. The profiling results are consistent with nanobench timing data (same functions identified as hot)
**Plans**: 1 plan in 1 wave

Plans:
- [ ] 03.1-01-PLAN.md -- Run macOS CPU sampling profiler and document ranked hot function list (Wave 1)

### Phase 4: Data Structure Modernization
**Goal**: Core data structures use contiguous memory with O(1) index access, eliminating per-lookup string hashing and per-append allocation
**Depends on**: Phase 2, Phase 3
**Requirements**: DATA-01, DATA-02, DATA-03
**Success Criteria** (what must be TRUE):
  1. Time-series fields in cpu_info, mem_info, and net_info use enum-indexed arrays instead of unordered_map<string, ...> -- Cachegrind confirms improved cache hit rate
  2. Graph history data uses fixed-size ring buffers instead of deque<long long> -- zero heap allocations during steady-state data collection
  3. Small fixed-key map instances (graph symbol lookup, config bools) use sorted vectors or flat arrays -- nanobench confirms faster lookup than unordered_map for these key sets
**Plans**: 5 plans in 5 waves

Plans:
- [x] 04-01-PLAN.md -- RingBuffer template + enum definitions + unit tests + graph_symbols/Graph::graphs migration (Wave 1)
- [x] 04-02-PLAN.md -- Migrate collector structs + all 5 platform collectors + draw code to enum-indexed RingBuffer arrays (Wave 2)
- [x] 04-03-PLAN.md -- Config enum migration: bools/ints/strings to enum-indexed arrays + 436 call site updates (Wave 3)
- [ ] 04-04-PLAN.md -- Gap closure: Fix BSD temperature/core_percent ring buffer bugs + bench_draw.cpp RingBuffer migration (Wave 4)
- [ ] 04-05-PLAN.md -- Gap closure: nanobench data structure comparison + measurement evidence documentation (Wave 5)

### Phase 5: Rendering Pipeline
**Goal**: Terminal output per frame is minimized to only changed content, with I/O batched into a single write
**Depends on**: Phase 2, Phase 4
**Requirements**: REND-01, REND-02, REND-03
**Success Criteria** (what must be TRUE):
  1. Differential terminal output is implemented -- only cells that changed between frames emit escape sequences, and strace confirms 30%+ reduction in bytes written per frame during steady-state operation
  2. All terminal output for a frame is flushed in a single write() call (or writev scatter-gather) instead of multiple small writes
  3. Graph rendering caches braille/tty characters for unchanged data points -- perf confirms graph draw time is reduced when only the newest data point changes
**Plans**: 3 plans in 2 waves

Plans:
- [ ] 05-01-PLAN.md -- write_stdout() POSIX helper + replace all cout output sites (Wave 1)
- [ ] 05-02-PLAN.md -- Graph column caching: skip computation when data.back() unchanged (Wave 1)
- [ ] 05-03-PLAN.md -- Cell buffer + escape-string parser + differential terminal output (Wave 2)

### Phase 6: Compiler & Verification
**Goal**: Compiler-level optimizations are applied to the fully-optimized codebase, and the entire optimization effort is verified correct
**Depends on**: Phase 5
**Requirements**: BILD-01, BILD-02, BILD-03
**Success Criteria** (what must be TRUE):
  1. A PGO CMake workflow exists (pgo-generate and pgo-use targets) and hyperfine confirms 10%+ additional performance gain over the non-PGO optimized build
  2. Full ASan, TSan, and UBSan sanitizer sweeps pass cleanly on the optimized build -- zero new issues introduced by any optimization
  3. mimalloc has been benchmarked as a drop-in replacement via LD_PRELOAD, with results documented comparing against the default allocator on the optimized build
**Plans**: TBD

Plans:
- [ ] 06-01: TBD
- [ ] 06-02: TBD

### Phase 7: Benchmark Integration Fixes
**Goal**: Stale micro-benchmarks measure optimized code paths and CI benchmark runs fail visibly instead of silently
**Depends on**: Phase 3
**Requirements**: PROF-02, PROF-03, IODC-01 (strengthens existing satisfied requirements)
**Gap Closure**: Closes integration and flow gaps from v1.0 milestone audit
**Success Criteria** (what must be TRUE):
  1. bench_proc_collect.cpp sub-benchmarks use Tools::read_proc_file() instead of std::ifstream, measuring the Phase 3 optimized I/O path
  2. CI benchmark workflow removes || true from proc benchmark invocation or adds explicit failure reporting so zero-data runs are detected
  3. Proc::collect full benchmark handles Shared::init() failure gracefully with a clear skip message instead of silent try/catch swallow
**Plans**: 1 plan in 1 wave

Plans:
- [x] 07-01-PLAN.md -- Update proc sub-benchmarks to read_proc_file() + fix CI silent failure + structured skip reporting (Wave 1)

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7
Note: Phases 2 and 3 depend only on Phase 1 (not on each other) but are executed sequentially for clean attribution of performance gains.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Profiling & Baseline | 3/3 | Complete | 2026-02-27 |
| 2. String & Allocation Reduction | 2/2 | Complete | 2026-02-27 |
| 3. I/O & Data Collection | 2/2 | Complete | 2026-02-27 |
| 3.1 Profiling Gap Closure | 0/1 | Not started | - |
| 4. Data Structure Modernization | 3/5 | Gap Closure | - |
| 5. Rendering Pipeline | 0/0 | Not started | - |
| 6. Compiler & Verification | 0/0 | Not started | - |
| 7. Benchmark Integration Fixes | 1/1 | Complete | 2026-02-27 |
