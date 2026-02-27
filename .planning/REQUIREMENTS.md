# Requirements: btop Performance Optimization

**Defined:** 2026-02-27
**Core Value:** Achieve measurable, significant reductions in btop's own resource consumption while preserving every aspect of the user experience.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Profiling & Benchmarking

- [ ] **PROF-01**: Baseline benchmarks established for CPU usage, memory footprint, startup time, and frame render time across Linux, macOS, and FreeBSD
- [ ] **PROF-02**: Micro-benchmarks created for identified hot functions (Proc::collect, draw functions, Fx::uncolor, string utilities) using nanobench
- [ ] **PROF-03**: Automated performance regression detection integrated into CI pipeline

### String & Allocation Optimization

- [x] **STRN-01**: std::regex in Fx::uncolor() replaced with hand-written ANSI escape code parser (10-100x improvement target)
- [x] **STRN-02**: String-by-value parameters in utility functions (uresize, luresize, etc.) replaced with const ref or string_view
- [x] **STRN-03**: fmt::format calls in draw pipeline replaced with fmt::format_to to eliminate intermediate string allocations
- [x] **STRN-04**: String reserve() calls added to draw functions with accurate size estimates accounting for escape code overhead

### I/O & Data Collection Optimization

- [x] **IODC-01**: Linux Proc::collect() ifstream usage replaced with POSIX read() and stack buffers, eliminating 1,500-2,000+ heap-allocating file opens per update
- [x] **IODC-02**: Redundant fs::exists() calls removed from readfile() utility (eliminating double-stat)
- [ ] **IODC-03**: O(N^2) linear PID scan replaced with hash-based PID lookup
- [ ] **IODC-04**: Equivalent I/O optimizations applied to macOS and FreeBSD data collectors

### Data Structure Optimization

- [ ] **DATA-01**: unordered_map<string, ...> with fixed keys replaced by enum-indexed flat arrays (cpu_percent, Config::bools, etc.)
- [ ] **DATA-02**: deque<long long> for time-series data replaced with fixed-size ring buffers
- [ ] **DATA-03**: map/unordered_map instances replaced with sorted vectors where key sets are small and fixed

### Rendering Optimization

- [ ] **REND-01**: Differential terminal output implemented — only emit escape sequences for cells that changed between frames (30-50% draw time reduction target)
- [ ] **REND-02**: Terminal output batched into single write() call per frame instead of multiple small writes
- [ ] **REND-03**: Graph rendering optimized — braille/tty graph characters cached, recomputation avoided for unchanged data points

### Compiler & Build Optimization

- [ ] **BILD-01**: Profile-Guided Optimization (PGO) CMake workflow implemented (10-20% gain target)
- [ ] **BILD-02**: Full ASan/TSan/UBSan sanitizer sweep completed verifying no correctness regressions from optimizations
- [ ] **BILD-03**: mimalloc evaluated as drop-in allocator replacement, with benchmark comparison against default allocator

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Advanced Rendering

- **REND-04**: Retained-mode UI layer for complex widgets (if differential output proves insufficient)
- **REND-05**: SIMD-accelerated escape code generation (if profiling shows it's worth the complexity)

### Advanced I/O

- **IODC-05**: Parallel data collection for independent collectors (Cpu, Mem, Net) if profiling shows serial pipeline is a bottleneck
- **IODC-06**: io_uring for batched /proc reads on Linux 5.6+

### Build

- **BILD-04**: Profiling build presets in CMake for developer convenience
- **BILD-05**: Continuous benchmarking dashboard

## Out of Scope

| Feature | Reason |
|---------|--------|
| Language rewrite (Rust, Zig, etc.) | This is C++ optimization, not a port |
| UI/UX changes | Explicitly excluded — no visual modifications |
| New feature additions | Scope is pure optimization |
| Custom global memory allocator | SSO handles most strings; mimalloc evaluation is sufficient |
| Lock-free data structures | No contention to eliminate in current threading model |
| mmap for /proc files | Virtual filesystem files don't benefit from mmap |
| Changing default update frequency | User-visible behavior must not change |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PROF-01 | Phase 1: Profiling & Baseline | Pending |
| PROF-02 | Phase 1: Profiling & Baseline | Pending |
| PROF-03 | Phase 1: Profiling & Baseline | Pending |
| STRN-01 | Phase 2: String & Allocation Reduction | Complete |
| STRN-02 | Phase 2: String & Allocation Reduction | Complete |
| STRN-03 | Phase 2: String & Allocation Reduction | Complete |
| STRN-04 | Phase 2: String & Allocation Reduction | Complete |
| IODC-01 | Phase 3: I/O & Data Collection | Complete |
| IODC-02 | Phase 3: I/O & Data Collection | Complete |
| IODC-03 | Phase 3: I/O & Data Collection | Pending |
| IODC-04 | Phase 3: I/O & Data Collection | Pending |
| DATA-01 | Phase 4: Data Structure Modernization | Pending |
| DATA-02 | Phase 4: Data Structure Modernization | Pending |
| DATA-03 | Phase 4: Data Structure Modernization | Pending |
| REND-01 | Phase 5: Rendering Pipeline | Pending |
| REND-02 | Phase 5: Rendering Pipeline | Pending |
| REND-03 | Phase 5: Rendering Pipeline | Pending |
| BILD-01 | Phase 6: Compiler & Verification | Pending |
| BILD-02 | Phase 6: Compiler & Verification | Pending |
| BILD-03 | Phase 6: Compiler & Verification | Pending |

**Coverage:**
- v1 requirements: 20 total
- Mapped to phases: 20
- Unmapped: 0

---
*Requirements defined: 2026-02-27*
*Last updated: 2026-02-27 after roadmap creation*
