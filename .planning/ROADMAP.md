# Roadmap: btop Optimization & Architecture

## Milestones

- ✅ **v1.0 Performance Optimization** — Phases 1-8 (shipped 2026-02-27)
- ✅ **v1.1 Automata Architecture** — Phases 10-15 (shipped 2026-03-01)
- ✅ **v1.2 Tech Debt** — Phases 16-19 (shipped 2026-03-02)
- [ ] **v1.3 Menu PDA + Input FSM** — Phases 20-25 (in progress)

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

### v1.3 Menu PDA + Input FSM (In Progress)

**Milestone Goal:** Replace the menu system's implicit bitset-driven state with an explicit pushdown automaton (typed stack frames, push/pop/replace), and refactor input handling into a typed finite state machine (Normal/Filtering/MenuActive). Zero user-visible behavior change.

**Phase Numbering:** Integer phases 20-25. Decimal phases (e.g., 20.1) reserved for urgent insertions.

- [x] **Phase 20: PDA Types + Skeleton** - Define MenuFrameVar variant, frame structs, PDAAction enum, and MenuPDA class with push/pop/replace/top/empty operations (completed 2026-03-02)
- [x] **Phase 21: Static Local Migration** - Refactor all 8 menu functions to receive frame data by reference, replacing function-static locals with frame struct members (completed 2026-03-02)
- [x] **Phase 22: PDA Dispatch** - Replace menuMask/currentMenu/menuFunc with std::visit dispatch on PDA top frame; eliminate Switch re-entrant call pattern (completed 2026-03-02)
- [x] **Phase 23: Input FSM** - Define InputStateVar variant, wire all transitions, replace if(filtering)/if(Menu::active) routing with FSM dispatch, integrate with App FSM resize path (completed 2026-03-02)
- [ ] **Phase 24: Tests** - PDA transition tests, Input FSM transition tests, integration scenarios, sanitizer-clean builds
- [ ] **Phase 25: Cleanup** - Remove menuMask, Menu::active, old_filter global, function-static locals; all migration scaffolding deleted

## Phase Details

### Phase 20: PDA Types + Skeleton
**Goal**: All typed data structures for the menu pushdown automaton exist and are unit-testable in isolation, with no behavior change to the running application
**Depends on**: Phase 19 (v1.2 complete)
**Requirements**: PDA-01, PDA-02, PDA-03, PDA-05, FRAME-02, FRAME-04, FRAME-05
**Success Criteria** (what must be TRUE):
  1. MenuFrameVar std::variant compiles with alternatives for all 8 menus (Main, Options, Help, SizeError, SignalChoose, SignalSend, SignalReturn, Renice)
  2. MenuPDA push/pop/replace/top/empty operations work correctly on an isolated stack (verified by a smoke test or static assertions)
  3. Frame handlers return PDAAction enum values (Push/Pop/Replace/NoChange) — no stack mutation occurs inside std::visit
  4. Each frame struct separates layout fields (x, y, width, height, bg) from interaction fields (selected, page, entered text), with constructors that unconditionally initialize all fields
  5. Per-frame mouse_mappings are owned as value members of each frame struct
**Plans**: 1 plan

Plans:
- [ ] 20-01-PLAN.md — PDA types, frame structs, MenuPDA class, and unit tests

### Phase 21: Static Local Migration
**Goal**: All 8 menu rendering functions consume per-frame state from frame structs instead of function-static locals, while menuMask/currentMenu remain the dispatch authority during migration
**Depends on**: Phase 20
**Requirements**: FRAME-01, FRAME-03
**Success Criteria** (what must be TRUE):
  1. Every menu function receives its frame struct by reference and reads/writes struct members instead of function-static locals
  2. invalidate_layout() zeros layout fields while preserving interaction fields (resize behavior preserved)
  3. A single-writer wrapper (menu_show/menu_close) updates both menuMask and PDA stack atomically, with assert(menuMask.none() == pda_stack.empty()) guard active during migration
  4. btop builds and runs with identical user-visible behavior — all menus open, navigate, and close correctly
**Plans**: 2 plans

Plans:
- [ ] 21-01-PLAN.md — invalidate_layout() on frame structs, file-scope PDA instance, single-writer wrappers
- [ ] 21-02-PLAN.md — Migrate all 8 menu functions from static locals to frame struct access

### Phase 22: PDA Dispatch
**Goal**: The PDA is the sole dispatch authority for all menu rendering — menuMask, currentMenu, and menuFunc are fully replaced by std::visit on PDA top frame
**Depends on**: Phase 21
**Requirements**: PDA-04, PDA-06
**Success Criteria** (what must be TRUE):
  1. Menu::process() dispatches to the active menu via std::visit on PDA top frame — menuMask bitset, currentMenu int, and menuFunc vector are deleted
  2. Main-to-Options and Main-to-Help transitions use replace() (ESC returns to Normal, not Main)
  3. bg string lifecycle is tied to stack depth: captured on first push, cleared on final pop
  4. The Switch return-code re-entrant call pattern is eliminated — all frame transitions return PDAAction values applied by the caller after the visitor completes
**Plans**: 2 plans

Plans:
- [ ] 22-01-PLAN.md — PDAResult struct, handler signature conversion, MenuVisitor, dispatch_legacy bridge
- [ ] 22-02-PLAN.md — Non-recursive process() loop, show() rewrite, legacy dispatch deletion, header cleanup

### Phase 23: Input FSM
**Goal**: Input routing is governed by a typed InputStateVar FSM that delegates to PDA when in MenuActive state, replacing all boolean-flag-based routing
**Depends on**: Phase 22
**Requirements**: INPUT-01, INPUT-02, INPUT-03, INPUT-04, INPUT-05, INPUT-06, INTEG-01, INTEG-02, INTEG-03, INTEG-04
**Success Criteria** (what must be TRUE):
  1. InputStateVar std::variant with Normal, Filtering, and MenuActive states is the sole authority for input routing — Input::process() dispatches via FSM, not if(filtering)/if(Menu::active) branches
  2. Menu::active atomic bool is removed — InputStateVar is the single source of truth for whether a menu is active
  3. Filtering state carries old_filter as a struct member; the file-scoped old_filter global in btop_input.cpp is removed
  4. Mouse routing in Input::get() reads InputStateVar instead of Menu::active bool; only the top PDA frame's mouse_mappings are active
  5. App FSM on_enter(Resizing) calls menu_pda.invalidate_layout() and Runner::pause_output is cleared on all paths that empty the PDA stack
**Plans**: 2 plans

Plans:
- [ ] 23-01-PLAN.md — InputStateVar types, transition functions, FSM dispatch rewrite in process()/get(), main loop replacement
- [ ] 23-02-PLAN.md — Menu::active removal, Menu::invalidate_layout() wrapper, integration wiring (resize, runner thread, draw)

### Phase 24: Tests
**Goal**: Comprehensive test coverage validates PDA invariants, Input FSM transitions, and integration scenarios, with all sanitizers clean
**Depends on**: Phase 23
**Requirements**: TEST-01, TEST-02, TEST-03, TEST-04, TEST-05, TEST-06, TEST-07
**Success Criteria** (what must be TRUE):
  1. PDA transition tests pass: push/pop/replace invariants, frame isolation (Options data does not bleed into Help), reopen produces fresh state
  2. Input FSM transition tests pass: all Normal<->Filtering and Normal<->MenuActive transitions, key routing per state, mouse routing delegates correctly
  3. SizeError override test passes (push SizeError over existing menu, pop returns to previous menu)
  4. SignalSend->SignalReturn sequence test passes (post-pop push pattern works correctly)
  5. Resize-with-menu-open preserves interaction fields and invalidates layout fields; all Filtering exit paths tested (ESC, mouse click, enter)
**Plans**: 2 plans

Plans:
- [ ] 24-01-PLAN.md — PDA invariant tests (frame isolation, reopen-fresh, SizeError override, signal sequence, resize) + Input FSM tests (transitions, filtering exit paths)
- [ ] 24-02-PLAN.md — ASan+UBSan and TSan sanitizer builds with full test suite

### Phase 25: Cleanup
**Goal**: All migration scaffolding, dead declarations, and superseded globals are removed — the codebase contains only the final PDA + Input FSM representation
**Depends on**: Phase 24
**Requirements**: CLEAN-01, CLEAN-02, CLEAN-03, CLEAN-04
**Success Criteria** (what must be TRUE):
  1. menuMask bitset, currentMenu int, and menuFunc dispatch vector are deleted from the source
  2. Menu::active atomic bool declaration is removed from btop_menu.hpp
  3. old_filter file-scope variable is removed from btop_input.cpp
  4. All function-static locals in the 8 menu functions are removed; grep for "static " in menu function bodies returns zero hits
**Plans**: TBD

Plans:
- [ ] 25-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 20 -> 20.1 (if inserted) -> 21 -> ... -> 25

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-8 | v1.0 | 20/20 | Complete | 2026-02-27 |
| 10-15 | v1.1 | 13/13 | Complete | 2026-03-01 |
| 16-19 | v1.2 | 4/4 | Complete | 2026-03-02 |
| 20. PDA Types + Skeleton | 1/1 | Complete    | 2026-03-02 | - |
| 21. Static Local Migration | 2/2 | Complete   | 2026-03-02 | - |
| 22. PDA Dispatch | 2/2 | Complete    | 2026-03-02 | - |
| 23. Input FSM | 2/2 | Complete    | 2026-03-02 | - |
| 24. Tests | 1/2 | In Progress|  | - |
| 25. Cleanup | v1.3 | 0/1 | Not started | - |
