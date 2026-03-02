# Milestones

## v1.2 Tech Debt (Shipped: 2026-03-02)

**Phases:** 4 | **Plans:** 4 | **Tasks:** 10 | **Source files:** 10 | **LOC:** +686 / -145
**Timeline:** 1 day (2026-03-01)
**Git range:** `20e2ddd`..`42f58c5`
**Tests:** 266/266 pass (zero sanitizer findings across 3 configs)

**Key accomplishments:**
1. Routed runner error paths through event queue (ThreadError push + on_event dispatch), eliminating direct shadow atomic writes
2. Established single-writer invariant: transition_to() is the sole writer of Global::app_state (grep-verifiable)
3. Routed SIGTERM through event queue like all other signals; Input/Menu quit paths via event::Quit
4. Fixed pre-existing RingBuffer.PushBackOnZeroCapacity test failure (266/266 tests now pass)
5. Measured cumulative v1.0+v1.1+v1.2 performance: -4.4% wall time, -6.3% collect, +13.7% RSS (deliberate pre-allocation)

**Archive:** `milestones/v1.2-ROADMAP.md` | `milestones/v1.2-REQUIREMENTS.md` | `milestones/v1.2-MILESTONE-AUDIT.md`

---

## v1.1 Automata Architecture (Shipped: 2026-03-01)

**Phases:** 6 | **Plans:** 13 | **Commits:** 66 | **Source files:** 63 | **LOC:** +13,095 / -272
**Timeline:** 2 days (2026-02-28 → 2026-03-01)
**Git range:** `3c0f2a9`..`27c5e28`
**Tests:** 279 total (278/279 pass, 1 pre-existing)

**Key accomplishments:**
1. Replaced 7 scattered atomic<bool> flags with explicit AppStateTag enum + AppStateVar std::variant (compile-time safe states)
2. Built lock-free SPSC EventQueue with async-signal-safe push, decoupling all signal handlers from main loop
3. Migrated main loop from flag-polling to event-driven dispatch (event_queue.try_pop → dispatch_event → on_event overloads)
4. Graduated states to std::variant with per-state data and entry/exit actions (transition_to + on_enter/on_exit)
5. Created independent Runner thread FSM (RunnerStateTag: Idle/Collecting/Drawing/Stopping) with typed cross-thread communication
6. Verified with 279 tests across 3 sanitizer configs (ASan+UBSan+TSan), zero findings, behavior human-approved

**Archive:** `milestones/v1.1-ROADMAP.md` | `milestones/v1.1-REQUIREMENTS.md` | `milestones/v1.1-MILESTONE-AUDIT.md`

---

## v1.0 Performance Optimization (Shipped: 2026-02-27)

**Phases:** 9 | **Plans:** 20 | **Commits:** 100 | **Source files:** 32 | **LOC:** +4,373 / -1,445
**Timeline:** 2026-02-27 (single day, ~14 hours)
**Git range:** `55bcf07`..`cf112a1`

**Key accomplishments:**
1. Replaced all hash map lookups with enum-indexed arrays (35-132x speedup for critical access patterns)
2. Built zero-allocation POSIX I/O pipeline replacing ifstream with read_proc_file() + write_stdout()
3. Created benchmark infrastructure (nanobench microbenchmarks + --benchmark CLI mode + CI regression detection)
4. Modernized deque+unordered_map to RingBuffer+arrays across 5 platform collectors
5. Implemented differential terminal rendering with cell-buffer optimization (~185x cache hit speedup)
6. Verified zero regressions via ASan/UBSan/TSan sweeps + PGO/mimalloc evaluation

**Archive:** `milestones/v1.0-ROADMAP.md` | `milestones/v1.0-REQUIREMENTS.md` | `milestones/v1.0-MILESTONE-AUDIT.md`

---

