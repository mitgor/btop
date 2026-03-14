# Requirements: btop Advanced Performance

**Defined:** 2026-03-13
**Core Value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.

## v1.7 Requirements

Requirements for advanced performance milestone. Each maps to roadmap phases.

### Memory & Allocation

- [x] **MEM-01**: Runner thread uses arena allocator (pmr::monotonic_buffer_resource) reset each collect/draw cycle
- [x] **MEM-02**: Draw functions write to pre-allocated output buffer via fmt::format_to instead of string concatenation
- [x] **MEM-03**: All /proc and sysctl parsing uses string_view — zero intermediate string copies
- [x] **MEM-04**: mimalloc evaluated as default allocator; adopted if benchmark shows >3% gain

### Compiler & Build

- [x] **BUILD-01**: PGO training workload covers filtering, sorting, menu interactions, resize, and idle
- [x] **BUILD-02**: CMake BTOP_NATIVE option enables -march=native for user-compiled builds
- [x] **BUILD-03**: Precompiled headers and/or unity build enabled for faster compilation

### Algorithmic

- [x] **ALG-01**: Process list uses partial sort (std::partial_sort) when display count < total processes
- [x] **ALG-02**: Remaining lookup tables moved to constexpr (theme, keys, escape sequences)
- [x] **ALG-03**: Process sort keys extracted to SoA layout for cache-friendly sorting

### Platform I/O

- [ ] **IO-01**: Linux /proc reads batched via io_uring with fallback to sequential POSIX I/O
- [ ] **IO-02**: macOS SMC/IOKit connection handles cached across collection cycles
- [ ] **IO-03**: Terminal output uses writev() scatter-gather I/O — single syscall per frame

## Future Requirements

Deferred beyond v1.7.

- **SIMD-01**: SIMD integer/delimiter parsing for /proc files
- **BOLT-01**: BOLT post-link optimization for Linux binaries

## Out of Scope

| Feature | Reason |
|---------|--------|
| UI/UX changes | Architecture-only project, no visual modifications |
| C++20 modules | Cross-platform support not mature enough |
| C++20 coroutines | Overhead exceeds benefit for small /proc file reads |
| Huge pages | Working set too small (~13MB) for TLB pressure |
| Polly optimizer | No dense numerical loops to optimize |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| MEM-01 | Phase 37 | Complete |
| MEM-02 | Phase 38 | Complete |
| MEM-03 | Phase 37 | Complete |
| MEM-04 | Phase 37 | Complete |
| BUILD-01 | Phase 35 | Complete |
| BUILD-02 | Phase 35 | Complete |
| BUILD-03 | Phase 35 | Complete |
| ALG-01 | Phase 36 | Complete |
| ALG-02 | Phase 36 | Complete |
| ALG-03 | Phase 36 | Complete |
| IO-01 | Phase 39 | Pending |
| IO-02 | Phase 39 | Pending |
| IO-03 | Phase 38 | Pending |

**Coverage:**
- v1.7 requirements: 13 total
- Mapped to phases: 13
- Unmapped: 0

---
*Requirements defined: 2026-03-13*
*Last updated: 2026-03-13 after roadmap creation*
