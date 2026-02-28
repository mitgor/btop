# Roadmap: btop Optimization & Architecture

## Milestones

- ✅ **v1.0 Performance Optimization** — Phases 1-8 (shipped 2026-02-27)
- 🚧 **v1.1 Automata Architecture** — Phases 10-15 (in progress)

## Phases

<details>
<summary>v1.0 Performance Optimization (Phases 1-8) — SHIPPED 2026-02-27</summary>

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

### v1.1 Automata Architecture

**Milestone Goal:** Replace btop's implicit boolean-flag state machines with explicit finite automata using std::variant + std::visit, enabling compile-time state safety, testable transitions, and decoupled event handling.

- [ ] **Phase 10: Name States** - Replace 7 atomic<bool> flags with an enum and create btop_state.hpp header
- [ ] **Phase 11: Event Queue** - Introduce typed event system and lock-free queue for signal decoupling
- [ ] **Phase 12: Extract Transitions** - Replace if/else if chain with typed transition functions and event-driven main loop
- [ ] **Phase 13: Type-Safe States** - Graduate enum to std::variant with per-state data and entry/exit actions
- [ ] **Phase 14: Runner FSM** - Extract runner thread's atomic flags into independent state machine
- [ ] **Phase 15: Verification** - Unit tests for both FSMs, sanitizer sweeps, and behavior preservation validation

## Phase Details

### Phase 10: Name States
**Goal**: App states have explicit names instead of scattered boolean flags
**Depends on**: Nothing (first phase of v1.1)
**Requirements**: STATE-01, STATE-04
**Success Criteria** (what must be TRUE):
  1. A single enum replaces all 7 atomic<bool> flags (resized, quitting, should_quit, should_sleep, _runner_started, init_conf, reload_conf)
  2. State types are defined in a new btop_state.hpp header that other translation units can include
  3. The main loop's if/else if chain uses enum comparison instead of boolean flag checks
  4. btop builds and runs identically on all platforms (zero behavior change)
**Plans**: TBD

### Phase 11: Event Queue
**Goal**: Signal handlers communicate via typed events instead of mutating global atomic flags
**Depends on**: Phase 10
**Requirements**: EVENT-01, EVENT-02, EVENT-03
**Success Criteria** (what must be TRUE):
  1. Events are defined as std::variant of typed structs (TimerTick, KeyInput, Resize, Signal, ConfigReload, ThreadError)
  2. A lock-free event queue exists for signal handler to main loop communication (signal-safe push, main-thread pop)
  3. All signal handlers (SIGWINCH, SIGINT, SIGTSTP, SIGCONT, etc.) push events into the queue instead of setting atomic flags
  4. btop responds to all signals identically to before (resize, suspend/resume, quit all work the same)
**Plans**: TBD

### Phase 12: Extract Transitions
**Goal**: State transition logic is explicit, typed, and the main loop is event-driven
**Depends on**: Phase 11
**Requirements**: TRANS-01, TRANS-02, TRANS-04, EVENT-04
**Success Criteria** (what must be TRUE):
  1. The main loop consumes events from the queue and dispatches to transition functions (no more direct flag polling)
  2. Each state+event combination has a named transition function (on_event overloads) instead of inline if/else if logic
  3. std::visit dispatches (state, event) pairs to the correct transition function
  4. All existing priority semantics are preserved (thread_exception > quit > sleep > reload > resize > timer > input)
  5. btop handles all edge cases identically (rapid resize, signal during menu, etc.)
**Plans**: TBD

### Phase 13: Type-Safe States
**Goal**: Illegal state combinations are compile-time errors and states carry their own data
**Depends on**: Phase 12
**Requirements**: STATE-02, STATE-03, TRANS-03
**Success Criteria** (what must be TRUE):
  1. AppState is a std::variant (Running, InMenu, Resizing, Sleeping, Quitting, Error) -- not an enum
  2. Each state carries its own data (Running holds timing info, Quitting holds exit code, Error holds message)
  3. Being in two states simultaneously is impossible (variant holds exactly one alternative)
  4. Entry/exit actions fire on state transitions (e.g., calcSizes on entering Resizing, timer reset on entering Running)
**Plans**: TBD

### Phase 14: Runner FSM
**Goal**: Runner thread has its own independent state machine replacing atomic flag coordination
**Depends on**: Phase 13
**Requirements**: RUNNER-01, RUNNER-02, RUNNER-03, RUNNER-04
**Success Criteria** (what must be TRUE):
  1. RunnerState is a std::variant (Idle, Collecting, Drawing, Stopping) -- independent from App FSM
  2. Runner's atomic<bool> flags (active, stopping, waiting, redraw) are replaced by FSM states
  3. Main thread communicates with runner via typed events, not flag mutation
  4. Binary semaphore wake-up pattern is preserved (no performance regression in runner wake latency)
  5. Data collection and drawing work identically (same metrics, same timing, same output)
**Plans**: TBD

### Phase 15: Verification
**Goal**: Both FSMs are proven correct through tests and sanitizer sweeps with zero regressions
**Depends on**: Phase 14
**Requirements**: VERIFY-01, VERIFY-02, VERIFY-03, VERIFY-04, VERIFY-05
**Success Criteria** (what must be TRUE):
  1. Unit tests cover every valid (state, event) pair for the App FSM (each transition tested)
  2. Unit tests cover every valid (state, event) pair for the Runner FSM (each transition tested)
  3. ASan/UBSan sweep passes with zero findings (no memory errors or undefined behavior introduced)
  4. TSan sweep passes with zero findings (no data races introduced by event queue or FSM transitions)
  5. All existing functionality works unchanged -- same visuals, same behavior, same defaults, same keyboard shortcuts
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 10 -> 11 -> 12 -> 13 -> 14 -> 15

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 10. Name States | 1/2 | In Progress|  | - |
| 11. Event Queue | v1.1 | 0/TBD | Not started | - |
| 12. Extract Transitions | v1.1 | 0/TBD | Not started | - |
| 13. Type-Safe States | v1.1 | 0/TBD | Not started | - |
| 14. Runner FSM | v1.1 | 0/TBD | Not started | - |
| 15. Verification | v1.1 | 0/TBD | Not started | - |
