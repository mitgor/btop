# Requirements: btop Automata Architecture

**Defined:** 2026-02-28
**Core Value:** Evolve btop's architecture toward explicit, testable state machines that eliminate invalid state combinations while preserving every aspect of the user experience.

## v1.1 Requirements

Requirements for automata architecture milestone. Each maps to roadmap phases.

### State Definition

- [ ] **STATE-01**: App states are named via enum replacing 7 scattered atomic<bool> flags
- [ ] **STATE-02**: App states carry per-state data via std::variant (Running holds timing, Quitting holds exit code, etc.)
- [ ] **STATE-03**: Illegal state combinations are unrepresentable at compile time (can't be Running and Quitting simultaneously)
- [ ] **STATE-04**: State types defined in new btop_state.hpp header

### Event System

- [ ] **EVENT-01**: Events defined as std::variant of typed event structs (TimerTick, KeyInput, Resize, Signal, etc.)
- [ ] **EVENT-02**: Lock-free event queue for signal handler -> main loop communication
- [ ] **EVENT-03**: Signal handlers push events instead of mutating global atomic flags
- [ ] **EVENT-04**: Main loop consumes events from queue and dispatches to transition functions

### Transition Logic

- [ ] **TRANS-01**: Transition functions extracted from if/else if chain into typed on_event() overloads
- [ ] **TRANS-02**: std::visit dispatches (state, event) pairs to correct transition function
- [ ] **TRANS-03**: Entry/exit actions execute on state transitions (e.g., calcSizes on entering Resizing)
- [ ] **TRANS-04**: All existing state transition semantics preserved (priority ordering, guard conditions)

### Runner FSM

- [ ] **RUNNER-01**: Runner thread states defined as std::variant (Idle, Collecting, Drawing, Stopping)
- [ ] **RUNNER-02**: Runner atomic<bool> flags (active, stopping, waiting, redraw) replaced by FSM states
- [ ] **RUNNER-03**: Main thread -> runner communication via typed events, not flag mutation
- [ ] **RUNNER-04**: Binary semaphore wake-up pattern preserved for performance

### Verification

- [ ] **VERIFY-01**: Unit tests for App FSM transitions (each state+event pair tested)
- [ ] **VERIFY-02**: Unit tests for Runner FSM transitions
- [ ] **VERIFY-03**: ASan/UBSan sweep confirms zero memory/UB regressions
- [ ] **VERIFY-04**: TSan sweep confirms zero data race regressions
- [ ] **VERIFY-05**: All existing functionality unchanged (same visuals, same behavior, same defaults)

## Future Requirements

### Menu FSM (v1.2)

- **MENU-01**: Menu bitset<8> + return codes replaced with explicit pushdown automaton
- **MENU-02**: Menu state stack with push/pop semantics
- **MENU-03**: Menu transitions testable independently

### Input FSM (v1.2)

- **INPUT-01**: Input filtering state machine (Normal <-> Filtering <-> SignalSelection)
- **INPUT-02**: Input mode transitions explicit and testable

## Out of Scope

| Feature | Reason |
|---------|--------|
| External FSM libraries (Boost.SML) | std::variant is sufficient, no new dependencies policy |
| Menu system refactor | Working and complex -- defer to v1.2 |
| Input state machine refactor | Lower priority -- defer to v1.2 |
| UI/UX changes | Architecture-only milestone |
| Config system FSM | Config lock pattern is simple enough as-is |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| STATE-01 | Phase 10 | Pending |
| STATE-02 | Phase 13 | Pending |
| STATE-03 | Phase 13 | Pending |
| STATE-04 | Phase 10 | Pending |
| EVENT-01 | Phase 11 | Pending |
| EVENT-02 | Phase 11 | Pending |
| EVENT-03 | Phase 11 | Pending |
| EVENT-04 | Phase 12 | Pending |
| TRANS-01 | Phase 12 | Pending |
| TRANS-02 | Phase 12 | Pending |
| TRANS-03 | Phase 13 | Pending |
| TRANS-04 | Phase 12 | Pending |
| RUNNER-01 | Phase 14 | Pending |
| RUNNER-02 | Phase 14 | Pending |
| RUNNER-03 | Phase 14 | Pending |
| RUNNER-04 | Phase 14 | Pending |
| VERIFY-01 | Phase 15 | Pending |
| VERIFY-02 | Phase 15 | Pending |
| VERIFY-03 | Phase 15 | Pending |
| VERIFY-04 | Phase 15 | Pending |
| VERIFY-05 | Phase 15 | Pending |

**Coverage:**
- v1.1 requirements: 21 total
- Mapped to phases: 21
- Unmapped: 0

---
*Requirements defined: 2026-02-28*
*Last updated: 2026-02-28 after roadmap creation*
