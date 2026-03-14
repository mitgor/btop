# Roadmap: btop Optimization & Architecture

## Milestones

- ✅ **v1.0 Performance Optimization** — Phases 1-8 (shipped 2026-02-27)
- ✅ **v1.1 Automata Architecture** — Phases 10-15 (shipped 2026-03-01)
- ✅ **v1.2 Tech Debt** — Phases 16-19 (shipped 2026-03-02)
- ✅ **v1.3 Menu PDA + Input FSM** — Phases 20-24 (shipped 2026-03-02)
- ✅ **v1.4 Render & Collect Modernization** — Phases 25-27 (shipped 2026-03-02)
- ✅ **v1.5 Render & Collect Completion** — Phases 28-29 (shipped 2026-03-03)
- ✅ **v1.6 Unified Redraw** — Phases 31-34 (shipped 2026-03-08)
- 🚧 **v1.7 Advanced Performance** — Phases 35-39 (in progress)

## Phases

<details>
<summary>v1.0 Performance Optimization (Phases 1-8) -- SHIPPED 2026-02-27</summary>

- [x] Phase 1: Profiling & Baseline (3/3 plans) -- completed 2026-02-27
- [x] Phase 2: String & Allocation Reduction (2/2 plans) -- completed 2026-02-27
- [x] Phase 3: I/O & Data Collection (2/2 plans) -- completed 2026-02-27
- [x] Phase 3.1: Profiling Gap Closure (1/1 plan) -- completed 2026-02-27
- [x] Phase 4: Data Structure Modernization (5/5 plans) -- completed 2026-02-27
- [x] Phase 5: Rendering Pipeline (3/3 plans) -- completed 2026-02-27
- [x] Phase 6: Compiler & Verification (2/2 plans) -- completed 2026-02-27
- [x] Phase 7: Benchmark Integration Fixes (1/1 plan) -- completed 2026-02-27
- [x] Phase 8: CI Coverage & Documentation Cleanup (1/1 plan) -- completed 2026-02-27

Full details: `milestones/v1.0-ROADMAP.md`

</details>

<details>
<summary>v1.1 Automata Architecture (Phases 10-15) -- SHIPPED 2026-03-01</summary>

- [x] Phase 10: Name States (2/2 plans) -- completed 2026-02-28
- [x] Phase 11: Event Queue (2/2 plans) -- completed 2026-02-28
- [x] Phase 12: Extract Transitions (2/2 plans) -- completed 2026-02-28
- [x] Phase 13: Type-Safe States (3/3 plans) -- completed 2026-02-28
- [x] Phase 14: Runner FSM (2/2 plans) -- completed 2026-03-01
- [x] Phase 15: Verification (2/2 plans) -- completed 2026-03-01

Full details: `milestones/v1.1-ROADMAP.md`

</details>

<details>
<summary>v1.2 Tech Debt (Phases 16-19) -- SHIPPED 2026-03-02</summary>

- [x] Phase 16: Runner Error Path Purity (1/1 plan) -- completed 2026-03-01
- [x] Phase 17: Signal & Transition Routing (1/1 plan) -- completed 2026-03-01
- [x] Phase 18: Test & Doc Hygiene (1/1 plan) -- completed 2026-03-01
- [x] Phase 19: Performance Measurement (1/1 plan) -- completed 2026-03-01

Full details: `milestones/v1.2-ROADMAP.md`

</details>

<details>
<summary>v1.3 Menu PDA + Input FSM (Phases 20-24) -- SHIPPED 2026-03-02</summary>

- [x] Phase 20: PDA Types + Skeleton (1/1 plan) -- completed 2026-03-02
- [x] Phase 21: Static Local Migration (2/2 plans) -- completed 2026-03-02
- [x] Phase 22: PDA Dispatch (2/2 plans) -- completed 2026-03-02
- [x] Phase 23: Input FSM (2/2 plans) -- completed 2026-03-02
- [x] Phase 24: Tests (2/2 plans) -- completed 2026-03-02

Full details: `milestones/v1.3-ROADMAP.md` | `milestones/v1.3-REQUIREMENTS.md`

</details>

<details>
<summary>v1.4 Render & Collect Modernization (Phases 25-27) -- SHIPPED 2026-03-02</summary>

- [x] Phase 25: Stale Static Const Fix (1/1 plan) -- completed 2026-03-02
- [x] Phase 26: ThemeKey Enum Arrays (2/2 plans) -- completed 2026-03-02
- [x] Phase 27: cpu_old Enum Arrays (2/2 plans) -- completed 2026-03-02

Full details: `milestones/v1.4-ROADMAP.md` | `milestones/v1.4-REQUIREMENTS.md`

</details>

<details>
<summary>v1.5 Render & Collect Completion (Phases 28-29) -- SHIPPED 2026-03-03</summary>

- [x] Phase 28: Hot-Path POSIX I/O (2/2 plans) -- completed 2026-03-02
- [x] Phase 29: Draw Decomposition (2/2 plans) -- completed 2026-03-02

Full details: `milestones/v1.5-ROADMAP.md` | `milestones/v1.5-REQUIREMENTS.md`

</details>

<details>
<summary>v1.6 Unified Redraw (Phases 31-34) -- SHIPPED 2026-03-08</summary>

- [x] Phase 31: DirtyFlags Foundation (2/2 plans) -- completed 2026-03-07
- [x] Phase 32: Runner Integration (1/1 plan) -- completed 2026-03-08
- [x] Phase 33: calcSizes Decoupling (1/1 plan) -- completed 2026-03-08
- [x] Phase 34: Per-Box Bool Migration (1/1 plan) -- completed 2026-03-08

Full details: `milestones/v1.6-ROADMAP.md` | `milestones/v1.6-REQUIREMENTS.md`

</details>

### v1.7 Advanced Performance (In Progress)

**Milestone Goal:** Push btop's performance further through compiler optimizations, algorithmic improvements, memory architecture (arena allocators, pre-allocated buffers), and platform-specific I/O.

- [x] **Phase 35: Build & Compiler** - PGO training, -march=native, PCH/unity build (completed 2026-03-13)
- [x] **Phase 36: Algorithmic Improvements** - Partial sort, constexpr tables, SoA sort keys (completed 2026-03-14)
- [x] **Phase 37: Allocation & Parsing** - Arena allocator, string_view audit, mimalloc evaluation (completed 2026-03-14)
- [ ] **Phase 38: Output Pipeline** - Pre-allocated draw buffer, writev scatter-gather output
- [ ] **Phase 39: Platform I/O** - io_uring Linux batching, macOS IOKit caching

## Phase Details

### Phase 35: Build & Compiler
**Goal**: Faster builds and better-optimized binaries without changing runtime code
**Depends on**: Nothing (first phase of v1.7)
**Requirements**: BUILD-01, BUILD-02, BUILD-03
**Success Criteria** (what must be TRUE):
  1. PGO training workload exercises filtering, sorting, menu interactions, resize, and idle — not just `--benchmark 50`
  2. User can pass `-DBTOP_NATIVE=ON` to CMake and get a binary compiled with `-march=native`
  3. Clean build time is measurably reduced via PCH and/or unity build configuration
**Plans**: 2 plans
Plans:
- [ ] 35-01-PLAN.md — PGO training workload (--pgo-training CLI flag + comprehensive training)
- [ ] 35-02-PLAN.md — CMake options (BTOP_NATIVE + BTOP_PCH)

### Phase 36: Algorithmic Improvements
**Goal**: Reduce CPU work per cycle through smarter algorithms and compile-time computation
**Depends on**: Phase 35 (PGO retraining benefits from algorithmic changes being in place, but not strictly required — can run in parallel)
**Requirements**: ALG-01, ALG-02, ALG-03
**Success Criteria** (what must be TRUE):
  1. Process list with 1000+ entries sorts faster when only 50 rows are displayed (partial sort avoids full O(P log P))
  2. Theme color tables, key mappings, and escape sequence building blocks are constexpr — no runtime initialization
  3. Process sort operation shows improved cache behavior via SoA key extraction (measurable via benchmark)
**Plans**: 3 plans
Plans:
- [x] 36-01-PLAN.md — Sort benchmark + partial sort (ALG-01)
- [x] 36-02-PLAN.md — Constexpr migration for escape sequences, lookup tables, config arrays (ALG-02)
- [ ] 36-03-PLAN.md — SoA key extraction for cache-friendly numeric sort (ALG-03)

### Phase 37: Allocation & Parsing
**Goal**: Eliminate per-cycle heap allocation churn through arena allocation, zero-copy parsing, and allocator evaluation
**Depends on**: Phase 36 (algorithmic changes should land first so allocation patterns are stable before arena work)
**Requirements**: MEM-01, MEM-03, MEM-04
**Success Criteria** (what must be TRUE):
  1. Runner thread's per-cycle allocations use pmr::monotonic_buffer_resource reset each cycle — no per-object deallocation
  2. All /proc and sysctl parsing paths use string_view for field extraction — no intermediate std::string copies for substring operations
  3. mimalloc evaluated via benchmark; adopted as CMake option if >3% gain demonstrated, otherwise documented as not worth it
  4. All existing tests pass with arena allocator active and string_view parsing changes in place
**Plans**: 3 plans
Plans:
- [ ] 37-01-PLAN.md — Arena allocator setup in runner loop + unit tests (MEM-01)
- [ ] 37-02-PLAN.md — string_view audit for CPU stat, battery, sensor, CPU freq parsing (MEM-03)
- [ ] 37-03-PLAN.md — mimalloc evaluation documentation + CMake option (MEM-04)

### Phase 38: Output Pipeline
**Goal**: Zero-copy output path from draw functions to terminal — pre-allocated buffer replaces string concatenation, single syscall per frame
**Depends on**: Phase 37 (arena allocator provides the memory substrate for pre-allocated buffers)
**Requirements**: MEM-02, IO-03
**Success Criteria** (what must be TRUE):
  1. Draw functions write to a pre-allocated buffer via fmt::format_to instead of returning std::string and concatenating with +=
  2. Terminal output uses writev() scatter-gather I/O — multiple buffer segments written in a single syscall per frame
  3. Draw path benchmark shows measurable improvement (fewer allocations, fewer syscalls)
**Plans**: 2 plans
Plans:
- [ ] 38-01-PLAN.md — Pre-allocated output buffer for draw functions (MEM-02)
- [ ] 38-02-PLAN.md — writev() scatter-gather I/O for terminal output (IO-03)

### Phase 39: Platform I/O
**Goal**: Platform-specific I/O optimizations that reduce syscall overhead on Linux and macOS collection paths
**Depends on**: Phase 37 (string_view parsing from Phase 37 is the foundation for processing io_uring results)
**Requirements**: IO-01, IO-02
**Success Criteria** (what must be TRUE):
  1. Linux builds batch /proc file reads via io_uring with automatic fallback to sequential POSIX I/O on older kernels (<5.1)
  2. macOS builds cache SMC/IOKit connection handles across collection cycles instead of re-opening each cycle
  3. Collection benchmark on each platform shows reduced syscall count and/or wall time
**Plans**: TBD

## Progress

**Execution Order:** 35 -> 36 -> 37 -> 38 -> 39

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 35. Build & Compiler | 2/2 | Complete    | 2026-03-13 |
| 36. Algorithmic Improvements | 3/3 | Complete    | 2026-03-14 |
| 37. Allocation & Parsing | 3/3 | Complete    | 2026-03-14 |
| 38. Output Pipeline | 1/2 | In Progress|  |
| 39. Platform I/O | 0/? | Not started | - |
