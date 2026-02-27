# Roadmap: btop Performance Optimization

## Overview

This roadmap delivers measurable performance improvements to btop++ across CPU usage, memory footprint, startup time, and rendering speed. The approach is strictly evidence-driven: establish baselines first, then attack bottlenecks in order of impact and risk, moving from safe leaf-level changes (strings, I/O) through structural changes (data structures, rendering pipeline) to compiler-level optimization last. All 20 v1 requirements are covered across 6 phases. Every optimization must be justified by profiling data and must preserve the full user experience unchanged.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Profiling & Baseline** - Establish reproducible benchmarks and identify actual bottlenecks before any code changes
- [ ] **Phase 2: String & Allocation Reduction** - Eliminate per-frame string allocation overhead in utilities and draw path
- [ ] **Phase 3: I/O & Data Collection** - Replace expensive ifstream/filesystem operations with POSIX I/O in collectors
- [ ] **Phase 4: Data Structure Modernization** - Replace hash maps and deques with enum-indexed arrays and ring buffers
- [ ] **Phase 5: Rendering Pipeline** - Implement differential terminal output and batch writes for minimal redraw
- [ ] **Phase 6: Compiler & Verification** - Apply PGO, evaluate mimalloc, and verify correctness with full sanitizer sweep

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
- [ ] 01-01-PLAN.md -- Benchmark build infrastructure + string/draw/proc micro-benchmarks (Wave 1)
- [ ] 01-02-PLAN.md -- Add --benchmark CLI mode for end-to-end measurement (Wave 1)
- [ ] 01-03-PLAN.md -- CI performance regression detection workflow (Wave 2)

### Phase 2: String & Allocation Reduction
**Goal**: Per-frame heap allocation count in the draw path and string utilities is measurably reduced, with regex hot path eliminated
**Depends on**: Phase 1
**Requirements**: STRN-01, STRN-02, STRN-03, STRN-04
**Success Criteria** (what must be TRUE):
  1. Fx::uncolor() uses a hand-written ANSI escape parser instead of std::regex, and heaptrack confirms the regex allocation spike is gone
  2. String utility functions (uresize, luresize, ljust, rjust, cjust) accept const ref or string_view parameters -- no copies on call
  3. Draw functions use fmt::format_to with back_inserter instead of fmt::format, eliminating intermediate string allocations in the render path
  4. Draw functions call string::reserve() with size estimates that account for escape code overhead, and heaptrack shows reduced reallocation count per frame
**Plans**: TBD

Plans:
- [ ] 02-01: TBD
- [ ] 02-02: TBD

### Phase 3: I/O & Data Collection
**Goal**: System data collection uses minimal syscalls and zero heap-allocating file I/O, with O(1) PID lookup replacing linear scan
**Depends on**: Phase 1
**Requirements**: IODC-01, IODC-02, IODC-03, IODC-04
**Success Criteria** (what must be TRUE):
  1. Linux Proc::collect() reads /proc files via POSIX read() with stack buffers -- strace confirms ifstream heap allocations are eliminated and syscall count per update is reduced by 40%+
  2. readfile() no longer calls fs::exists() before opening -- double-stat eliminated for every file read
  3. PID lookup in current_procs uses a hash-based container -- process collection time scales linearly (not quadratically) with process count
  4. macOS and FreeBSD collectors have equivalent optimizations applied to their respective APIs (sysctl, proc_pidinfo, kvm_getprocs)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD
- [ ] 03-02: TBD

### Phase 4: Data Structure Modernization
**Goal**: Core data structures use contiguous memory with O(1) index access, eliminating per-lookup string hashing and per-append allocation
**Depends on**: Phase 2, Phase 3
**Requirements**: DATA-01, DATA-02, DATA-03
**Success Criteria** (what must be TRUE):
  1. Time-series fields in cpu_info, mem_info, and net_info use enum-indexed arrays instead of unordered_map<string, ...> -- Cachegrind confirms improved cache hit rate
  2. Graph history data uses fixed-size ring buffers instead of deque<long long> -- zero heap allocations during steady-state data collection
  3. Small fixed-key map instances (graph symbol lookup, config bools) use sorted vectors or flat arrays -- nanobench confirms faster lookup than unordered_map for these key sets
**Plans**: TBD

Plans:
- [ ] 04-01: TBD
- [ ] 04-02: TBD

### Phase 5: Rendering Pipeline
**Goal**: Terminal output per frame is minimized to only changed content, with I/O batched into a single write
**Depends on**: Phase 2, Phase 4
**Requirements**: REND-01, REND-02, REND-03
**Success Criteria** (what must be TRUE):
  1. Differential terminal output is implemented -- only cells that changed between frames emit escape sequences, and strace confirms 30%+ reduction in bytes written per frame during steady-state operation
  2. All terminal output for a frame is flushed in a single write() call (or writev scatter-gather) instead of multiple small writes
  3. Graph rendering caches braille/tty characters for unchanged data points -- perf confirms graph draw time is reduced when only the newest data point changes
**Plans**: TBD

Plans:
- [ ] 05-01: TBD
- [ ] 05-02: TBD

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

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6
Note: Phases 2 and 3 depend only on Phase 1 (not on each other) but are executed sequentially for clean attribution of performance gains.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Profiling & Baseline | 0/3 | Planned | - |
| 2. String & Allocation Reduction | 0/0 | Not started | - |
| 3. I/O & Data Collection | 0/0 | Not started | - |
| 4. Data Structure Modernization | 0/0 | Not started | - |
| 5. Rendering Pipeline | 0/0 | Not started | - |
| 6. Compiler & Verification | 0/0 | Not started | - |
