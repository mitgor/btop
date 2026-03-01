# Roadmap: btop Optimization & Architecture

## Milestones

- ✅ **v1.0 Performance Optimization** — Phases 1-8 (shipped 2026-02-27)
- ✅ **v1.1 Automata Architecture** — Phases 10-15 (shipped 2026-03-01)
- **v1.2 Tech Debt** — Phases 16-19 (in progress)

## Phases

<details>
<summary>✅ v1.0 Performance Optimization (Phases 1-8) — SHIPPED 2026-02-27</summary>

- [x] Phase 1: Profiling & Baseline (3/3 plans) — completed 2026-02-27
- [x] Phase 2: String & Allocation Reduction (2/2 plans) — completed 2026-02-27
- [x] Phase 3: I/O & Data Collection (2/2 plans) — completed 2026-02-27
- [x] Phase 3.1: Profiling Gap Closure (1/1 plan) — completed 2026-02-27
- [x] Phase 4: Data Structure Modernization (5/5 plans) — completed 2026-02-27
- [x] Phase 5: Rendering Pipeline (3/3 plans) — completed 2026-02-27
- [x] Phase 6: Compiler & Verification (2/2 plans) — completed 2026-02-27
- [x] Phase 7: Benchmark Integration Fixes (1/1 plan) — completed 2026-02-27
- [x] Phase 8: CI Coverage & Documentation Cleanup (1/1 plan) — completed 2026-02-27

Full details: `milestones/v1.0-ROADMAP.md`

</details>

<details>
<summary>✅ v1.1 Automata Architecture (Phases 10-15) — SHIPPED 2026-03-01</summary>

- [x] Phase 10: Name States (2/2 plans) — completed 2026-02-28
- [x] Phase 11: Event Queue (2/2 plans) — completed 2026-02-28
- [x] Phase 12: Extract Transitions (2/2 plans) — completed 2026-02-28
- [x] Phase 13: Type-Safe States (3/3 plans) — completed 2026-02-28
- [x] Phase 14: Runner FSM (2/2 plans) — completed 2026-03-01
- [x] Phase 15: Verification (2/2 plans) — completed 2026-03-01

Full details: `milestones/v1.1-ROADMAP.md`

</details>

### v1.2 Tech Debt (In Progress)

**Milestone Goal:** Close all v1.1 integration impurities, fix pre-existing issues, route all state transitions through the FSM architecture, and measure actual CPU/memory improvements.

- [ ] **Phase 16: Runner Error Path Purity** - Runner exception handling uses event queue end-to-end with variant/shadow sync
- [ ] **Phase 17: Signal & Transition Routing** - All remaining shadow atomic bypasses routed through FSM transition_to()
- [ ] **Phase 18: Test & Doc Hygiene** - Pre-existing test failure fixed and stale documentation cleaned up
- [ ] **Phase 19: Performance Measurement** - CPU and memory improvements from v1.0+v1.1 quantified with documented methodology

## Phase Details

### Phase 16: Runner Error Path Purity
**Goal**: Runner thread errors flow through the event-driven architecture instead of bypassing it with direct shadow atomic writes
**Depends on**: Phase 15 (v1.1 FSM infrastructure)
**Requirements**: PURE-01, PURE-02, PURE-03, PURE-04
**Success Criteria** (what must be TRUE):
  1. Runner exception handler pushes event::ThreadError into the event queue (no direct atomic writes on error path)
  2. Main loop receives and dispatches ThreadError events via on_event(Running, ThreadError)
  3. runner_var is either used for actual runtime dispatch in the runner thread or completely removed (no write-only dead code remains)
  4. When runner reports an error, app_var holds state::Error and the shadow atomic agrees (variant and shadow are in sync)
  5. All error-path changes pass ASan+UBSan and TSan sweeps with zero findings
**Plans**: 1 plan
- [ ] 16-01-PLAN.md — Route runner errors through event queue, remove runner_var dead code, sanitizer verification

### Phase 17: Signal & Transition Routing
**Goal**: Every state transition in btop goes through transition_to(), eliminating the last shadow atomic bypasses
**Depends on**: Phase 16
**Requirements**: PURE-05, PURE-06
**Success Criteria** (what must be TRUE):
  1. term_resize() triggers state changes via transition_to() instead of writing shadow atomics directly
  2. clean_quit() triggers state changes via transition_to() instead of writing shadow atomics directly
  3. SIGTERM is pushed into the event queue and dispatched through on_event(), not handled by direct flag writes
  4. No code path outside transition_to() writes to shadow atomic state variables (grep-verifiable)
**Plans**: TBD

### Phase 18: Test & Doc Hygiene
**Goal**: Pre-existing test failures and stale documentation from earlier milestones are cleaned up
**Depends on**: Nothing (independent of Phases 16-17)
**Requirements**: HYGN-01, HYGN-02
**Success Criteria** (what must be TRUE):
  1. RingBuffer.PushBackOnZeroCapacity test passes in all build configs (normal, ASan+UBSan, TSan)
  2. VERIFICATION.md contains no references to removed Phase 12 infrastructure or other stale content
  3. Full test suite runs 279/279 pass (the pre-existing failure is resolved)
**Plans**: TBD

### Phase 19: Performance Measurement
**Goal**: Quantify the actual CPU and memory impact of v1.0+v1.1 optimizations with reproducible methodology
**Depends on**: Phases 16-17 (measure after all code changes are complete)
**Requirements**: PERF-01, PERF-02
**Success Criteria** (what must be TRUE):
  1. CPU usage is measured using a documented, reproducible methodology (tool, workload, duration, environment specified)
  2. Memory footprint is measured using a documented, reproducible methodology (tool, metric, workload specified)
  3. Before/after comparison shows measured values for both baseline (pre-v1.0) and current (post-v1.2) builds
  4. Results are committed to the repository with methodology documentation sufficient for independent reproduction
**Plans**: TBD

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-8 | v1.0 | 20/20 | Complete | 2026-02-27 |
| 10-15 | v1.1 | 13/13 | Complete | 2026-03-01 |
| 16. Runner Error Path Purity | v1.2 | 0/1 | Planned | - |
| 17. Signal & Transition Routing | v1.2 | 0/? | Not started | - |
| 18. Test & Doc Hygiene | v1.2 | 0/? | Not started | - |
| 19. Performance Measurement | v1.2 | 0/? | Not started | - |
