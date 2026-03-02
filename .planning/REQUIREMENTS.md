# Requirements: btop++ v1.5

**Defined:** 2026-03-02
**Core Value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.

## v1.5 Requirements

Requirements for v1.5 Render & Collect Completion. Carried forward from v1.4 phases 28-30.

### Hot-Path Performance

- [ ] **PERF-04**: Cpu::collect /proc/stat reads use read_proc_file() instead of ifstream (Linux)
- [ ] **PERF-05**: Mem::collect /proc/meminfo reads use read_proc_file() instead of ifstream (Linux)

### Draw Decomposition

- [ ] **DRAW-01**: Proc::draw() split into focused sub-functions (detailed view, list rendering, filter display)
- [ ] **DRAW-02**: Cpu::draw() split with battery state tracking extracted into separate function
- [ ] **DRAW-03**: No regression in rendered output -- visual diff confirms identical terminal output

### Render Architecture

- [ ] **REND-01**: Scattered redraw booleans (Cpu::redraw, Mem::redraw, Net::redraw, Proc::redraw, Gpu::redraw) consolidated into unified mechanism
- [ ] **REND-02**: All existing redraw trigger sites (calcSizes, input, collect, draw) work through unified mechanism

## Future Requirements

Deferred to future milestone. Tracked but not in current roadmap.

### Input Modernization

- **INPT-01**: Input::process() 438-line if-else cascade replaced with typed key-dispatch table
- **INPT-02**: Key dispatch indexed by current InputState variant

### Collect Decomposition

- **COLL-01**: Cpu::collect() (875 lines) split into collect_cpu_times(), collect_battery(), collect_sensors()
- **COLL-02**: Mem::collect() (558 lines) split into sub-functions

### Config Layer

- **CONF-01**: Config::validError global side-channel replaced with returned error type
- **CONF-02**: Config::unlock() decoupled from Proc display state
- **CONF-03**: Proc::draw() Config::set() mutations replaced with render-output struct

## Out of Scope

| Feature | Reason |
|---------|--------|
| New state machines | v1.5 is completion work, not new FSM scope |
| Input dispatch rewrite | Deferred -- large scope, independent of render/collect work |
| Platform collector decomposition | Deferred -- large scope, cross-platform testing needed |
| Runner::pause_output refactor | Requires event queue changes, better as standalone work |
| UI/UX changes | Architecture only, no user-facing modifications |
| New dependencies | Prefer zero-cost or header-only if needed |
| BSD/macOS ifstream conversion | Only Linux has read_proc_file(); BSD/macOS use sysctl APIs |

## Traceability

Which phases cover which requirements. Carried forward from v1.4.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PERF-04 | Phase 28 | Pending |
| PERF-05 | Phase 28 | Pending |
| DRAW-01 | Phase 29 | Pending |
| DRAW-02 | Phase 29 | Pending |
| DRAW-03 | Phase 29 | Pending |
| REND-01 | Phase 30 | Pending |
| REND-02 | Phase 30 | Pending |

**Coverage:**
- v1.5 requirements: 7 total
- Mapped to phases: 7
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-02 (carried forward from v1.4)*
*Last updated: 2026-03-02 after v1.5 milestone start*
