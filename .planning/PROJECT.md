# btop Performance Optimization

## What This Is

A comprehensive performance optimization effort for btop++, the terminal-based system monitor. The v1.0 milestone delivered measurable improvements across CPU usage, memory allocation, I/O efficiency, data structure access, and rendering throughput — all without changing anything the user sees or does. The full visual experience, feature set, update frequency, and cross-platform support remain untouched.

## Core Value

Achieve measurable, significant reductions in btop's own resource consumption (CPU, RAM, startup latency, render time) while preserving every aspect of the user experience.

## Requirements

### Validated

- ✓ Profile-driven: identify actual bottlenecks before optimizing — v1.0 (nanobench + macOS CPU sampling)
- ✓ All optimizations must pass existing functionality unchanged — v1.0 (ASan/UBSan/TSan clean)
- ✓ Cross-platform: Linux, macOS, FreeBSD all benefit — v1.0 (5 platform collectors optimized)
- ✓ Improve rendering throughput (less time per frame, smoother updates) — v1.0 (differential cell buffer, graph caching)
- ✓ Reduce per-frame heap allocations via string/I/O optimization — v1.0 (POSIX I/O, format_to, RingBuffer)
- ✓ Enum-indexed arrays replace hash map lookups — v1.0 (35-132x speedup on critical paths)
- ✓ CI benchmark regression detection pipeline — v1.0 (github-action-benchmark + nanobench converter)

### Active

- [ ] Reduce btop's CPU usage by 50%+ during normal operation (infrastructure built, end-to-end measurement pending)
- [ ] Reduce btop's memory footprint by 30%+ (allocations reduced, total RSS measurement pending)
- [ ] Reduce startup time to first rendered frame (not specifically measured yet)

### Out of Scope

- UI/UX changes — no visual modifications, no feature additions or removals
- Changing default update frequency or refresh behavior
- Dropping platform support (Linux, macOS, FreeBSD all stay)
- Rewriting in another language — this is C++ optimization, not a port
- Adding new dependencies solely for performance (prefer zero-cost or header-only if needed)

## Context

- btop++ is a C++ terminal-based system monitor with rich Unicode UI (graphs, box-drawing, colors)
- Supports Linux, macOS, and FreeBSD
- Modern C++ features (C++20/23) used throughout — GCC 12+/Clang 15+ target
- v1.0 shipped: 9 phases, 20 plans, 100 commits, 32 source files modified (+4,373/-1,445 LOC)
- Benchmark infrastructure in place: nanobench microbenchmarks, --benchmark CLI mode, CI regression detection
- PGO build pipeline available (1.1% gain — I/O-bound ceiling); mimalloc evaluated (1.6% gain)
- Sanitizer sweeps clean (ASan/UBSan/TSan) — zero regressions from all optimizations

## Constraints

- **Usability**: Zero regression in user-visible behavior — same features, same visuals, same defaults
- **Correctness**: All system metrics must remain accurate after optimization
- **Compatibility**: Must build and run on Linux, macOS, FreeBSD
- **Compiler**: Modern C++ ok (GCC 12+/Clang 15+), leverage language features where they help
- **Approach**: Profile-first — every optimization must be justified by profiling data

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Profile before optimizing | Avoid wasting effort on non-bottlenecks | ✓ Good — nanobench + macOS sampling guided all work |
| Modern C++ (20/23) allowed | Enables move semantics, constexpr, ranges, etc. | ✓ Good — constexpr arrays, string_view, structured bindings used throughout |
| No new heavy dependencies | Keep build simple, avoid dependency bloat | ✓ Good — only nanobench (header-only, test-only) added |
| All platforms must benefit | Not just Linux-specific tricks | ✓ Good — 5 platform collectors optimized, cross-platform write_stdout |
| Regex → hand-written parser | std::regex allocates on every call | ✓ Good — Fx::uncolor single-pass parser, zero allocations |
| POSIX read() over ifstream | ifstream heap-allocates, double-stats | ✓ Good — read_proc_file() with stack buffers |
| Enum-indexed arrays over hash maps | O(1) index vs hash+compare | ✓ Good — 35-132x speedup on config/data lookups |
| RingBuffer over deque | Zero steady-state allocations | ✓ Good — comparable throughput, no heap churn |
| Differential rendering | Only emit changed cells | ✓ Good — cell buffer with escape-string parser |
| PGO as final phase | Profile optimized code, not original | ✓ Good — 1.1% gain (I/O-bound ceiling documented) |

---
*Last updated: 2026-02-27 after v1.0 milestone*
