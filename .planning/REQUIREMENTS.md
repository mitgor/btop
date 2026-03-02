# Requirements: btop++ v1.4

**Defined:** 2026-03-02
**Core Value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.

## v1.4 Requirements

Requirements for v1.4 Render & Collect Modernization. Each maps to roadmap phases.

### Correctness

- [x] **CORR-01**: calcSizes() freq_range and hasCpuHz values refresh when config changes instead of being baked at first call

### Hot-Path Performance

- [x] **PERF-01**: cpu_old string-keyed unordered_map replaced with std::array<long long, CpuField::COUNT> on Linux
- [ ] **PERF-02**: cpu_old equivalent replaced with enum-indexed array on macOS
- [ ] **PERF-03**: cpu_old equivalent replaced with enum-indexed array on FreeBSD
- [ ] **PERF-04**: Cpu::collect /proc/stat reads use read_proc_file() instead of ifstream (Linux)
- [ ] **PERF-05**: Mem::collect /proc/meminfo reads use read_proc_file() instead of ifstream (Linux)
- [x] **PERF-06**: Theme colors map replaced with ThemeKey enum + std::array (~40 fixed color names)
- [x] **PERF-07**: Theme rgbs map replaced with ThemeKey enum + std::array
- [x] **PERF-08**: Theme gradients map replaced with ThemeKey enum + std::array

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
| New state machines | v1.4 is modernization, not new FSM work |
| Input dispatch rewrite | Deferred -- large scope, independent of render/collect work |
| Platform collector decomposition | Deferred -- large scope, cross-platform testing needed |
| Runner::pause_output refactor | Requires event queue changes, better as standalone work |
| UI/UX changes | Architecture only, no user-facing modifications |
| New dependencies | Prefer zero-cost or header-only if needed |
| BSD/macOS ifstream conversion | Only Linux has read_proc_file(); BSD/macOS use sysctl APIs |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CORR-01 | Phase 25 | Complete |
| PERF-01 | Phase 27 | Complete |
| PERF-02 | Phase 27 | Pending |
| PERF-03 | Phase 27 | Pending |
| PERF-04 | Phase 28 | Pending |
| PERF-05 | Phase 28 | Pending |
| PERF-06 | Phase 26 | Complete |
| PERF-07 | Phase 26 | Complete |
| PERF-08 | Phase 26 | Complete |
| DRAW-01 | Phase 29 | Pending |
| DRAW-02 | Phase 29 | Pending |
| DRAW-03 | Phase 29 | Pending |
| REND-01 | Phase 30 | Pending |
| REND-02 | Phase 30 | Pending |

**Coverage:**
- v1.4 requirements: 14 total
- Mapped to phases: 14
- Unmapped: 0

---
*Requirements defined: 2026-03-02*
*Last updated: 2026-03-02 after roadmap creation*
