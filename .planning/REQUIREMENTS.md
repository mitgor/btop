# Requirements: btop Unified Redraw

**Defined:** 2026-03-03
**Core Value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.

## v1.6 Requirements

Requirements for unified redraw milestone. Each maps to roadmap phases.

### DirtyFlags Infrastructure

- [x] **FLAG-01**: DirtyBit enum defined with per-box bits (Cpu, Mem, Net, Proc, Gpu) plus ForceFullEmit
- [x] **FLAG-02**: PendingDirty struct wraps atomic<uint32_t> with mark()/take() API using fetch_or/exchange
- [x] **FLAG-03**: Single DirtyBit::Gpu covers all GPU panels (replaces vector<bool>)
- [x] **FLAG-04**: Unit tests verify bit operations, mark/take semantics, and concurrent access

### Cleanup

- [x] **CLEAN-01**: Dead Proc::resized atomic<bool> removed from declaration and read site
- [x] **CLEAN-02**: calcSizes() guard simplified after Proc::resized removal
- [x] **CLEAN-03**: Local `bool redraw` vars in btop_input.cpp renamed to `force_redraw`

### Runner Integration

- [x] **WIRE-01**: pending_redraw atomic<bool> replaced by PendingDirty instance
- [x] **WIRE-02**: runner_conf::force_redraw replaced by per-box dirty bits from take()
- [x] **WIRE-03**: Draw functions receive per-box dirty state instead of single force_redraw bool
- [x] **WIRE-04**: ScreenBuffer::force_full driven by ForceFullEmit bit, kept separate from per-box bits

### calcSizes() Decoupling

- [ ] **DECPL-01**: calcSizes() uses mark(All) instead of direct per-namespace bool assignment
- [ ] **DECPL-02**: All 5 calcSizes() call sites verified to produce correct dirty state
- [ ] **DECPL-03**: request_redraw() uses mark() on PendingDirty instead of separate atomic

### Per-Box Migration

- [ ] **MIGR-01**: Cpu::redraw namespace bool removed, draw() derives state from dirty parameter
- [ ] **MIGR-02**: Mem::redraw namespace bool removed, draw() derives state from dirty parameter
- [ ] **MIGR-03**: Net::redraw namespace bool removed (self-invalidation on IP change preserved)
- [ ] **MIGR-04**: Proc::redraw namespace bool removed, draw() derives state from dirty parameter
- [ ] **MIGR-05**: Gpu::redraw vector<bool> removed, draw() derives state from dirty parameter
- [ ] **MIGR-06**: Per-box extern declarations removed from btop_shared.hpp

## Future Requirements

None — this milestone completes the unified redraw consolidation.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Sub-box dirty tracking | ScreenBuffer already handles cell-level differential rendering |
| Observer/listener pattern | Over-engineering for 5 boxes with simple dirty bits |
| Async dirty propagation | Single producer-consumer pattern is sufficient |
| Menu::redraw consolidation | Main-thread-only, no cross-thread concern, stays separate |
| Draw function signature changes | Conservative approach: keep existing signatures, pass dirty via parameter |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| FLAG-01 | Phase 31 | Complete |
| FLAG-02 | Phase 31 | Complete |
| FLAG-03 | Phase 31 | Complete |
| FLAG-04 | Phase 31 | Complete |
| CLEAN-01 | Phase 31 | Complete |
| CLEAN-02 | Phase 31 | Complete |
| CLEAN-03 | Phase 31 | Complete |
| WIRE-01 | Phase 32 | Complete |
| WIRE-02 | Phase 32 | Complete |
| WIRE-03 | Phase 32 | Complete |
| WIRE-04 | Phase 32 | Complete |
| DECPL-01 | Phase 33 | Pending |
| DECPL-02 | Phase 33 | Pending |
| DECPL-03 | Phase 33 | Pending |
| MIGR-01 | Phase 34 | Pending |
| MIGR-02 | Phase 34 | Pending |
| MIGR-03 | Phase 34 | Pending |
| MIGR-04 | Phase 34 | Pending |
| MIGR-05 | Phase 34 | Pending |
| MIGR-06 | Phase 34 | Pending |

**Coverage:**
- v1.6 requirements: 20 total
- Mapped to phases: 20
- Unmapped: 0

---
*Requirements defined: 2026-03-03*
*Last updated: 2026-03-07 after Phase 31 completion*
