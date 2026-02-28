# btop Optimization & Architecture

## What This Is

An ongoing optimization and architectural improvement effort for btop++, the terminal-based system monitor. v1.0 delivered measurable performance improvements (enum-indexed arrays, POSIX I/O, differential rendering, RingBuffer). v1.1 focuses on replacing btop's implicit flag-driven state management with explicit finite automata — making state transitions first-class, testable, and compile-time safe.

## Core Value

Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.

## Current Milestone: v1.1 Automata Architecture

**Goal:** Replace btop's implicit boolean-flag state machines with explicit finite automata using `std::variant` + `std::visit`, enabling compile-time state safety, testable transitions, and decoupled event handling.

**Target features:**
- Explicit App FSM replacing 7 scattered atomic<bool> flags
- Event queue decoupling signal handlers from main loop state
- Type-safe states via std::variant (illegal states = compile errors)
- Runner thread FSM replacing atomic flag coordination
- Testable transition logic (currently untestable due to global mutable state)

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

- [ ] Replace global atomic<bool> flags with explicit App FSM (std::variant)
- [ ] Introduce event queue for signal→main loop communication
- [ ] Extract transition logic from if/else if chain into typed transition functions
- [ ] Create Runner thread FSM with typed states (Idle/Collecting/Drawing/Stopping)
- [ ] Achieve testable state transitions (unit tests for FSM logic)
- [ ] Reduce btop's CPU usage by 50%+ during normal operation (from v1.0, measurement pending)
- [ ] Reduce btop's memory footprint by 30%+ (from v1.0, measurement pending)

### Out of Scope

- UI/UX changes — no visual modifications, no feature additions or removals
- Changing default update frequency or refresh behavior
- Dropping platform support (Linux, macOS, FreeBSD all stay)
- Rewriting in another language — this is C++ optimization, not a port
- Adding new dependencies solely for performance (prefer zero-cost or header-only if needed)
- Menu system refactor to pushdown automaton — defer to v1.2 (working, complex, lower priority)
- External FSM libraries (Boost.SML) — std::variant is sufficient, no new dependencies

## Context

- btop++ is a C++ terminal-based system monitor with rich Unicode UI (graphs, box-drawing, colors)
- Supports Linux, macOS, and FreeBSD
- Modern C++ features (C++20/23) used throughout — GCC 12+/Clang 15+ target
- v1.0 shipped: 9 phases, 20 plans, 100 commits, 32 source files modified (+4,373/-1,445 LOC)
- Benchmark infrastructure in place: nanobench microbenchmarks, --benchmark CLI mode, CI regression detection
- PGO build pipeline available (1.1% gain — I/O-bound ceiling); mimalloc evaluated (1.6% gain)
- Sanitizer sweeps clean (ASan/UBSan/TSan) — zero regressions from all optimizations
- Current architecture has 7 implicit state machines encoded as atomic<bool> flags and conditional chains
- Main loop (btop.cpp:1309-1385) checks flags in priority order — ordering is load-bearing but undocumented
- Research completed: `.planning/research/AUTOMATA-ARCHITECTURE.md` — std::variant approach validated

## Constraints

- **Usability**: Zero regression in user-visible behavior — same features, same visuals, same defaults
- **Correctness**: All system metrics must remain accurate after optimization
- **Compatibility**: Must build and run on Linux, macOS, FreeBSD
- **Compiler**: Modern C++ ok (GCC 12+/Clang 15+), leverage language features where they help
- **Approach**: Incremental migration — each phase ships independently, behavior preserved throughout
- **Dependencies**: No external FSM libraries — std::variant + std::visit only

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

| std::variant + std::visit for FSM | Zero dependencies, type-safe, fits C++20 style | — Pending |
| Orthogonal FSMs (App + Runner) | Avoids state explosion from flag combinations | — Pending |
| Event queue for signal decoupling | Unidirectional data flow, eliminates shared mutable state | — Pending |
| Phased migration (not big-bang) | Each phase independently deployable, reduces risk | — Pending |

---
*Last updated: 2026-02-28 after v1.1 milestone start*
