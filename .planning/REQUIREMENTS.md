# Requirements: btop Tech Debt Cleanup

**Defined:** 2026-03-01
**Core Value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.

## v1.2 Requirements

Requirements for tech debt cleanup milestone. Each maps to roadmap phases.

### FSM Integration Purity

- [x] **PURE-01**: Runner exception handler pushes event::ThreadError into event queue instead of writing shadow atomic directly
- [x] **PURE-02**: on_event(Running, ThreadError) is reachable and tested (no dead code)
- [x] **PURE-03**: RunnerStateVar is either used for runtime dispatch or removed (no write-only dead code)
- [x] **PURE-04**: When runner reports error, app_var transitions to state::Error (variant/shadow in sync)
- [x] **PURE-05**: term_resize() and clean_quit() use transition_to() instead of direct shadow atomic writes
- [x] **PURE-06**: SIGTERM routed through event system like other signals

### Test & Doc Hygiene

- [ ] **HYGN-01**: RingBuffer.PushBackOnZeroCapacity test passes (fix pre-existing failure)
- [ ] **HYGN-02**: Stale VERIFICATION.md references to removed Phase 12 infrastructure cleaned up

### Performance Measurement

- [ ] **PERF-01**: CPU usage measured before/after v1.0+v1.1 optimizations with documented methodology
- [ ] **PERF-02**: Memory footprint measured before/after v1.0+v1.1 optimizations with documented methodology

## Future Requirements

### Menu FSM (v1.3+)

- **MENU-01**: Menu bitset<8> + return codes replaced with explicit pushdown automaton
- **MENU-02**: Menu state stack with push/pop semantics
- **MENU-03**: Menu transitions testable independently

### Input FSM (v1.3+)

- **INPUT-01**: Input filtering state machine (Normal <-> Filtering <-> SignalSelection)
- **INPUT-02**: Input mode transitions explicit and testable

## Out of Scope

| Feature | Reason |
|---------|--------|
| Menu system refactor | Working and complex — defer to v1.3+ |
| Input state machine refactor | Lower priority — defer to v1.3+ |
| New features or UI changes | Cleanup-only milestone |
| External FSM libraries | std::variant is sufficient, no new dependencies |
| Runner loop redesign | Only fix integration impurities, preserve existing architecture |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| PURE-01 | Phase 16 | Complete |
| PURE-02 | Phase 16 | Complete |
| PURE-03 | Phase 16 | Complete |
| PURE-04 | Phase 16 | Complete |
| PURE-05 | Phase 17 | Complete |
| PURE-06 | Phase 17 | Complete |
| HYGN-01 | Phase 18 | Pending |
| HYGN-02 | Phase 18 | Pending |
| PERF-01 | Phase 19 | Pending |
| PERF-02 | Phase 19 | Pending |

**Coverage:**
- v1.2 requirements: 10 total
- Mapped to phases: 10
- Unmapped: 0

---
*Requirements defined: 2026-03-01*
*Last updated: 2026-03-01 after roadmap creation*
