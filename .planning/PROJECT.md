# btop Optimization & Architecture

## What This Is

An ongoing optimization and architectural improvement effort for btop++, the terminal-based system monitor. v1.0 delivered measurable performance improvements (enum-indexed arrays, POSIX I/O, differential rendering, RingBuffer). v1.1 replaced btop's implicit flag-driven state management with explicit finite automata — type-safe std::variant states, lock-free event queue, event-driven dispatch, and independent App + Runner FSMs with 279 tests.

## Core Value

Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.

## Requirements

### Validated

- ✓ Profile-driven: identify actual bottlenecks before optimizing — v1.0 (nanobench + macOS CPU sampling)
- ✓ All optimizations must pass existing functionality unchanged — v1.0 (ASan/UBSan/TSan clean)
- ✓ Cross-platform: Linux, macOS, FreeBSD all benefit — v1.0 (5 platform collectors optimized)
- ✓ Improve rendering throughput (less time per frame, smoother updates) — v1.0 (differential cell buffer, graph caching)
- ✓ Reduce per-frame heap allocations via string/I/O optimization — v1.0 (POSIX I/O, format_to, RingBuffer)
- ✓ Enum-indexed arrays replace hash map lookups — v1.0 (35-132x speedup on critical paths)
- ✓ CI benchmark regression detection pipeline — v1.0 (github-action-benchmark + nanobench converter)
- ✓ Replace global atomic<bool> flags with explicit App FSM (std::variant) — v1.1 (AppStateTag + AppStateVar)
- ✓ Introduce event queue for signal→main loop communication — v1.1 (lock-free SPSC EventQueue)
- ✓ Extract transition logic into typed transition functions — v1.1 (on_event overloads + dispatch_event)
- ✓ Create Runner thread FSM with typed states — v1.1 (RunnerStateTag: Idle/Collecting/Drawing/Stopping)
- ✓ Achieve testable state transitions — v1.1 (279 tests, 278/279 pass)

### Active

- [ ] Reduce btop's CPU usage by 50%+ during normal operation (from v1.0, measurement pending)
- [ ] Reduce btop's memory footprint by 30%+ (from v1.0, measurement pending)

### Out of Scope

- UI/UX changes — no visual modifications, no feature additions or removals
- Changing default update frequency or refresh behavior
- Dropping platform support (Linux, macOS, FreeBSD all stay)
- Rewriting in another language — this is C++ optimization, not a port
- Adding new dependencies solely for performance (prefer zero-cost or header-only if needed)
- Menu system refactor to pushdown automaton — candidate for v1.2 (working, complex)
- Input state machine refactor — candidate for v1.2 (lower priority)

## Context

- btop++ is a C++ terminal-based system monitor with rich Unicode UI (graphs, box-drawing, colors)
- Supports Linux, macOS, and FreeBSD
- Modern C++ features (C++20/23) used throughout — GCC 12+/Clang 15+ target
- v1.0 shipped: 9 phases, 20 plans, 100 commits, 32 source files modified (+4,373/-1,445 LOC)
- v1.1 shipped: 6 phases, 13 plans, 66 commits, 63 source files modified (+13,095/-272 LOC)
- Architecture now has explicit FSMs: App FSM (btop_state.hpp, btop.cpp) + Runner FSM (btop_state.hpp, btop_shared.hpp)
- Event-driven main loop: event_queue.try_pop() → dispatch_event() → on_event() → transition_to()
- 279 tests across normal, ASan+UBSan, TSan configs — zero sanitizer findings
- Benchmark infrastructure: nanobench microbenchmarks, --benchmark CLI mode, CI regression detection
- PGO build pipeline available (1.1% gain — I/O-bound ceiling); mimalloc evaluated (1.6% gain)
- Known tech debt: runner error path bypasses event queue, runner_var write-only, variant/shadow desync on error

## Constraints

- **Usability**: Zero regression in user-visible behavior — same features, same visuals, same defaults
- **Correctness**: All system metrics must remain accurate after optimization
- **Compatibility**: Must build and run on Linux, macOS, FreeBSD
- **Compiler**: Modern C++ ok (GCC 12+/Clang 15+), leverage language features where they help
- **Approach**: Incremental migration — each phase ships independently, behavior preserved throughout

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
| std::variant + std::visit for FSM | Zero dependencies, type-safe, fits C++20 style | ✓ Good — compile-time state safety, 279 tests |
| Orthogonal FSMs (App + Runner) | Avoids state explosion from flag combinations | ✓ Good — independent state machines, no 128-state product |
| Event queue for signal decoupling | Unidirectional data flow, eliminates shared mutable state | ✓ Good — lock-free SPSC, async-signal-safe |
| Phased migration (not big-bang) | Each phase independently deployable, reduces risk | ✓ Good — 6 phases, each shipped and tested incrementally |
| Shadow atomic for cross-thread state | Variant is main-thread only, atomic for cross-thread reads | ⚠️ Revisit — creates desync on error path (accepted tech debt) |

---
*Last updated: 2026-03-01 after v1.1 milestone*
