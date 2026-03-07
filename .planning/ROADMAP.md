# Roadmap: btop Optimization & Architecture

## Milestones

- ✅ **v1.0 Performance Optimization** — Phases 1-8 (shipped 2026-02-27)
- ✅ **v1.1 Automata Architecture** — Phases 10-15 (shipped 2026-03-01)
- ✅ **v1.2 Tech Debt** — Phases 16-19 (shipped 2026-03-02)
- ✅ **v1.3 Menu PDA + Input FSM** — Phases 20-24 (shipped 2026-03-02)
- ✅ **v1.4 Render & Collect Modernization** — Phases 25-27 (shipped 2026-03-02)
- ✅ **v1.5 Render & Collect Completion** — Phases 28-29 (shipped 2026-03-03)
- 🚧 **v1.6 Unified Redraw** — Phases 31-34 (in progress)

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

### v1.6 Unified Redraw (In Progress)

**Milestone Goal:** Consolidate scattered redraw flags into a unified dirty-flag mechanism, remove dead code, fix naming collisions, and decouple layout recomputation from redraw forcing.

- [x] **Phase 31: DirtyFlags Foundation** - DirtyBit enum, PendingDirty struct, dead code removal, naming cleanup
- [ ] **Phase 32: Runner Integration** - Wire PendingDirty into runner consumer and main-thread producers
- [ ] **Phase 33: calcSizes Decoupling** - Replace per-namespace bool assigns with mark(All)
- [ ] **Phase 34: Per-Box Bool Migration** - Remove all namespace redraw bools, migrate all write sites, full validation

## Phase Details

### Phase 31: DirtyFlags Foundation
**Goal**: The DirtyBit enum and PendingDirty atomic accumulator exist as a tested, compile-clean type; dead code and naming collisions are eliminated before any migration begins
**Depends on**: Nothing (first phase of v1.6)
**Requirements**: FLAG-01, FLAG-02, FLAG-03, FLAG-04, CLEAN-01, CLEAN-02, CLEAN-03
**Success Criteria** (what must be TRUE):
  1. `src/btop_dirty.hpp` exists with DirtyBit enum (Cpu, Mem, Net, Proc, Gpu, ForceFullEmit bits) and PendingDirty struct with mark()/take() API
  2. Unit tests verify bit operations (mark single, mark multiple, take clears, concurrent mark+take under TSan)
  3. `Proc::resized` atomic<bool> is gone from declaration and all read sites; `calcSizes()` guard condition simplified
  4. All four `bool redraw` local variables in `btop_input.cpp` are renamed to `force_redraw`
  5. Project compiles clean on all platforms with no behavioral change
**Plans**: 2 plans

Plans:
- [x] 31-01-PLAN.md — DirtyBit enum and PendingDirty struct with unit tests
- [x] 31-02-PLAN.md — Dead Proc::resized removal and force_redraw rename

### Phase 32: Runner Integration
**Goal**: The runner thread consumes dirty state from PendingDirty instead of the old pending_redraw/force_redraw mechanism; ForceFullEmit is correctly separated from per-box dirty bits
**Depends on**: Phase 31
**Requirements**: WIRE-01, WIRE-02, WIRE-03, WIRE-04
**Success Criteria** (what must be TRUE):
  1. `pending_redraw atomic<bool>` no longer exists; `runner_conf::force_redraw` field is removed
  2. Runner calls `pending_dirty.take()` once per draw cycle and derives per-box booleans passed to each `draw()` function
  3. `request_redraw()` calls `pending_dirty.mark(DirtyBit::All)` instead of `pending_redraw.store(true)`
  4. ForceFullEmit bit drives `screen_buffer.set_force_full()` exclusively; single-key-press cycles use differential emit, not full emit
  5. All 330+ tests pass; TSan clean
**Plans**: TBD

Plans:
- [ ] 32-01: TBD
- [ ] 32-02: TBD

### Phase 33: calcSizes Decoupling
**Goal**: calcSizes() marks all boxes dirty through PendingDirty instead of directly assigning per-namespace bool flags; geometry computation is decoupled from redraw forcing
**Depends on**: Phase 32
**Requirements**: DECPL-01, DECPL-02, DECPL-03
**Success Criteria** (what must be TRUE):
  1. `calcSizes()` body contains zero direct assignments to per-namespace redraw bools; uses `pending_dirty.mark(DirtyBit::All)` instead
  2. All 5 `calcSizes()` call sites (btop.cpp Runner::run, btop.cpp _runner resize, btop_draw.cpp banner, btop_input.cpp options, btop_menu.cpp screen_redraw) produce correct dirty state after the call
  3. Resize and config-reload paths trigger full box redraw through dirty bits (not through direct bool assignment)
**Plans**: TBD

Plans:
- [ ] 33-01: TBD

### Phase 34: Per-Box Bool Migration
**Goal**: All five per-box namespace redraw bools are removed; every write site across all platform collectors is migrated to PendingDirty; the entire redraw mechanism is unified and validated end-to-end
**Depends on**: Phase 33
**Requirements**: MIGR-01, MIGR-02, MIGR-03, MIGR-04, MIGR-05, MIGR-06
**Success Criteria** (what must be TRUE):
  1. `Cpu::redraw`, `Mem::redraw`, `Net::redraw`, `Proc::redraw` namespace bools and `Gpu::redraw vector<bool>` are all deleted; their extern declarations in `btop_shared.hpp` are removed
  2. All write sites across all platform-specific `btop_collect.cpp` files (Linux, macOS, FreeBSD, NetBSD, OpenBSD) use `pending_dirty.mark()` instead of direct bool assignment
  3. Self-invalidation inside draw functions is preserved (e.g., Net::draw IP-change trigger calls `pending_dirty.mark(DirtyBit::Net)`)
  4. `grep -rn "redraw\s*=\s*true" src/` returns zero namespace-bool hits; only Menu::redraw (out of scope) remains
  5. Full test suite passes (330+ tests); ASan/UBSan/TSan clean; manual verification of resize, IP change, sort toggle, filter change, and menu open/close redraw triggers
**Plans**: TBD

Plans:
- [ ] 34-01: TBD
- [ ] 34-02: TBD

## Progress

**Execution Order:** 31 -> 32 -> 33 -> 34

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 31. DirtyFlags Foundation | v1.6 | 2/2 | Complete | 2026-03-07 |
| 32. Runner Integration | v1.6 | 0/TBD | Not started | - |
| 33. calcSizes Decoupling | v1.6 | 0/TBD | Not started | - |
| 34. Per-Box Bool Migration | v1.6 | 0/TBD | Not started | - |
