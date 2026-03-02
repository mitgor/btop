# Roadmap: btop Optimization & Architecture

## Milestones

- ✅ **v1.0 Performance Optimization** — Phases 1-8 (shipped 2026-02-27)
- ✅ **v1.1 Automata Architecture** — Phases 10-15 (shipped 2026-03-01)
- ✅ **v1.2 Tech Debt** — Phases 16-19 (shipped 2026-03-02)
- ✅ **v1.3 Menu PDA + Input FSM** — Phases 20-24 (shipped 2026-03-02)
- [ ] **v1.4 Render & Collect Modernization** — Phases 25-30

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

### v1.4 Render & Collect Modernization

**Milestone Goal:** Modernize the render and collect layers -- enum-indexed theme colors, POSIX I/O for hot-path reads, decomposed god functions, unified redraw mechanism, and stale-state bug fix. Zero regression in user-visible behavior.

**Phase Numbering:** Integer phases 25-30. Decimal phases (e.g., 25.1) reserved for urgent insertions.

- [x] **Phase 25: Stale Static Const Fix** - Fix calcSizes() freq_range/hasCpuHz values that are baked at first call instead of refreshing on config change (completed 2026-03-02)
- [x] **Phase 26: ThemeKey Enum Arrays** - Replace theme string-keyed maps (colors, rgbs, gradients) with ThemeKey enum + std::array (completed 2026-03-02)
- [ ] **Phase 27: cpu_old Enum Arrays** - Convert cpu_old string-keyed unordered_map to enum-indexed std::array on all platforms
- [ ] **Phase 28: Hot-Path POSIX I/O** - Convert Cpu::collect and Mem::collect ifstream reads to read_proc_file() on Linux
- [ ] **Phase 29: Draw Decomposition** - Split Proc::draw() and Cpu::draw() god functions into focused sub-functions
- [ ] **Phase 30: Unified Redraw** - Consolidate scattered redraw booleans into a single mechanism with all trigger sites wired through it

## Phase Details

### Phase 25: Stale Static Const Fix
**Goal**: calcSizes() produces correct freq_range and hasCpuHz values after config changes, instead of returning stale values baked at first invocation
**Depends on**: Phase 24 (v1.3 complete)
**Requirements**: CORR-01
**Success Criteria** (what must be TRUE):
  1. Changing CPU frequency display settings and triggering a resize causes calcSizes() to recompute freq_range and hasCpuHz from current state
  2. Values that depend on runtime CPU data are recalculated on every calcSizes() call, not cached as static const
  3. btop builds on Linux, macOS, and FreeBSD with zero warnings; all existing tests pass
**Plans**: TBD

Plans:
- [ ] 25-01: TBD

### Phase 26: ThemeKey Enum Arrays
**Goal**: All theme color lookups use O(1) enum-indexed std::array access instead of string-keyed unordered_map hashing, across colors, rgbs, and gradients
**Depends on**: Phase 25
**Requirements**: PERF-06, PERF-07, PERF-08
**Success Criteria** (what must be TRUE):
  1. A ThemeKey enum exists with entries for all ~40 fixed color names, and the compiler rejects any lookup with an invalid key
  2. Theme::colors is a std::array<string, ThemeKey::COUNT> (or equivalent) -- no unordered_map for color lookups at runtime
  3. Theme::rgbs and Theme::gradients are std::array-backed with enum indexing -- no string hashing on the draw hot path
  4. Theme switching (user changes theme in options menu) populates the new arrays correctly; all colors render identically before and after
  5. btop builds and runs on all platforms with identical visual output -- no missing or wrong colors
**Plans**: 2/2 complete

Plans:
- [x] 26-01: Define ThemeKey Enums and Convert Theme Storage
- [x] 26-02: Migrate All Call Sites to Enum Keys

### Phase 27: cpu_old Enum Arrays
**Goal**: The cpu_old data structure uses enum-indexed std::array instead of string-keyed unordered_map on all three platforms
**Depends on**: Phase 25
**Requirements**: PERF-01, PERF-02, PERF-03
**Success Criteria** (what must be TRUE):
  1. On Linux, cpu_old is std::array<long long, CpuField::COUNT> and all /proc/stat field accesses use enum indices
  2. On macOS, the equivalent cpu_old structure uses enum-indexed array with platform-appropriate field names
  3. On FreeBSD, the equivalent cpu_old structure uses enum-indexed array with platform-appropriate field names
  4. CPU usage percentages displayed in btop match pre-change values (delta calculations produce identical results)
**Plans**: 2 plans

Plans:
- [ ] 27-01: Define CpuOldField Enum and Convert Linux cpu_old
- [ ] 27-02: Convert macOS/FreeBSD/OpenBSD/NetBSD cpu_old

### Phase 28: Hot-Path POSIX I/O
**Goal**: Linux hot-path proc file reads in Cpu::collect and Mem::collect use zero-allocation POSIX read() via read_proc_file() instead of ifstream
**Depends on**: Phase 25
**Requirements**: PERF-04, PERF-05
**Success Criteria** (what must be TRUE):
  1. Cpu::collect reads /proc/stat via read_proc_file() with a stack buffer -- no ifstream construction or heap allocation per collect cycle
  2. Mem::collect reads /proc/meminfo via read_proc_file() with a stack buffer -- no ifstream construction or heap allocation per collect cycle
  3. CPU and memory metrics displayed in btop are identical before and after the conversion (verified by comparing output values)
  4. macOS and FreeBSD builds are unaffected (their collectors use sysctl APIs, not /proc)
**Plans**: TBD

Plans:
- [ ] 28-01: TBD

### Phase 29: Draw Decomposition
**Goal**: Proc::draw() and Cpu::draw() god functions are split into focused sub-functions with no change in rendered output
**Depends on**: Phase 26, Phase 27, Phase 28
**Requirements**: DRAW-01, DRAW-02, DRAW-03
**Success Criteria** (what must be TRUE):
  1. Proc::draw() is decomposed into named sub-functions (detailed view rendering, process list rendering, filter bar display) with the top-level function under 100 lines
  2. Cpu::draw() is decomposed with battery state tracking extracted into a separate function; top-level function under 100 lines
  3. Terminal output is byte-identical before and after decomposition (verified by capturing and diffing draw output)
  4. All existing tests pass; btop builds on all three platforms
**Plans**: TBD

Plans:
- [ ] 29-01: TBD
- [ ] 29-02: TBD

### Phase 30: Unified Redraw
**Goal**: All redraw state is managed through a single mechanism instead of 5+ scattered boolean flags, with every existing trigger site wired through it
**Depends on**: Phase 29
**Requirements**: REND-01, REND-02
**Success Criteria** (what must be TRUE):
  1. Cpu::redraw, Mem::redraw, Net::redraw, Proc::redraw, and Gpu::redraw booleans are replaced by a unified redraw mechanism (e.g., a bitset, enum-indexed array, or single struct)
  2. All existing trigger sites (calcSizes, input handlers, collect functions, draw functions) set redraw through the unified mechanism -- grep for the old boolean names returns zero hits
  3. Resize triggers a full redraw of all boxes through the unified mechanism
  4. Normal update cycle redraws only the boxes whose collect data changed -- no over-invalidation
**Plans**: TBD

Plans:
- [ ] 30-01: TBD
- [ ] 30-02: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 25 -> 25.1 (if inserted) -> 26 -> ... -> 30

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-8 | v1.0 | 20/20 | Complete | 2026-02-27 |
| 10-15 | v1.1 | 13/13 | Complete | 2026-03-01 |
| 16-19 | v1.2 | 4/4 | Complete | 2026-03-02 |
| 20-24 | v1.3 | 9/9 | Complete | 2026-03-02 |
| 25. Stale Static Const Fix | 1/1 | Complete   | 2026-03-02 | - |
| 26. ThemeKey Enum Arrays | 2/2 | Complete   | 2026-03-02 | - |
| 27. cpu_old Enum Arrays | v1.4 | 0/2 | Not started | - |
| 28. Hot-Path POSIX I/O | v1.4 | 0/1 | Not started | - |
| 29. Draw Decomposition | v1.4 | 0/2 | Not started | - |
| 30. Unified Redraw | v1.4 | 0/2 | Not started | - |
