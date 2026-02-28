---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: Automata Architecture
status: unknown
last_updated: "2026-02-28T14:40:01.086Z"
progress:
  total_phases: 4
  completed_phases: 4
  total_plans: 8
  completed_plans: 8
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-28)

**Core value:** Evolve btop's architecture toward explicit, testable state machines that eliminate invalid state combinations while preserving every aspect of the user experience.
**Current focus:** Phase 13 - Type-Safe States (fourth phase of v1.1 Automata Architecture)

## Current Position

Phase: 13 of 15 (Type-Safe States)
Plan: 2 of 2 (13-02 complete)
Status: Phase Complete
Last activity: 2026-02-28 -- Completed 13-02 variant-based dispatch with entry/exit actions

Progress: [██████████] 100% (2/2 plans in phase 13)

## Performance Metrics

**Velocity:**
- Total plans completed: 8
- Average duration: 3.6min
- Total execution time: 0.47 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 10-name-states P01 | 1 tasks | 2min | 2min |
| 10-name-states P02 | 2 tasks | 5min | 5min |
| 11-event-queue P01 | 1 tasks | 4min | 4min |
| 11-event-queue P02 | 2 tasks | 4min | 4min |
| 13-type-safe-states P01 | 2 tasks | 5min | 5min |
| 13-type-safe-states P02 | 2 tasks | 5min | 5min |

## Accumulated Context

### Decisions

Full decision log in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- v1.1 scope: std::variant + std::visit, no external FSM libraries
- Migration is incremental -- each phase ships independently
- Menu system refactor deferred to v1.2 (working, complex, lower priority)
- Orthogonal FSMs (App + Runner) to avoid 128-state explosion
- [Phase 10-name-states]: Tests use local atomic<AppState> instances to avoid link dependency on btop.cpp
- [Phase 10-name-states]: AppState enum uses uint8_t underlying type for guaranteed lock-free atomics
- [Phase 10-name-states]: init_conf and _runner_started remain as atomic<bool> (lock and lifecycle marker, not app states)
- [Phase 10-name-states]: compare_exchange_strong used for safe state clearing back to Running
- [Phase 10-name-states]: Error state stores use memory_order_release paired with acquire in main loop
- [Phase 11-event-queue]: Power-of-2 capacity enforced via C++20 requires clause for bitwise modulo
- [Phase 11-event-queue]: Cache-line aligned (alignas(64)) head/tail to prevent false sharing
- [Phase 11-event-queue]: TimerTick and ThreadError included in variant for Phase 12 forward compatibility
- [Phase 11-event-queue]: Tests use local EventQueue instances -- no dependency on btop.cpp globals
- [Phase 11-event-queue]: Input::interrupt() called in signal handler (not in push) to keep queue generic
- [Phase 11-event-queue]: SIGUSR1 remains no-op pass-through (pselect interrupt, no event needed)
- [Phase 11-event-queue]: Early exit on Quitting/Error in drain loop preserves priority semantics
- [Phase 12-extract-transitions]: dispatch_event() declared in header, defined in btop.cpp (on_event overloads are static)
- [Phase 12-extract-transitions]: Catch-all on_event(auto, auto, current) preserves current state for all unmatched pairs
- [Phase 12-extract-transitions]: KeyInput uses fixed 32-byte buffer (31 chars max) for trivially copyable AppEvent
- [Phase 12-extract-transitions]: Side-effecting transitions (Quit, Sleep) inline Runner::stopping in on_event
- [Phase 13-type-safe-states]: state:: namespace placed outside Global:: for ergonomic variant type declarations
- [Phase 13-type-safe-states]: AppStateVar at file scope (main-thread only), distinct from cross-thread AppStateTag
- [Phase 13-type-safe-states]: to_tag() uses if-constexpr chain inside std::visit for compile-time exhaustiveness
- [Phase 13-type-safe-states]: Entry/exit actions placed after Runner namespace for correct forward declaration ordering
- [Phase 13-type-safe-states]: TransitionCtx struct for dependency injection (cli ref) into on_enter overloads
- [Phase 13-type-safe-states]: on_event overloads are pure value-returning functions; side effects in on_enter/on_exit
- [Phase 13-type-safe-states]: State chains (Reloading->Resizing->Running) via sequential transition_to() in main loop

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-28
Stopped at: Completed 13-02-PLAN.md (variant-based dispatch with entry/exit actions)
Resume file: None
