---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: Automata Architecture
status: in-progress
last_updated: "2026-02-28T14:26:01.771Z"
progress:
  total_phases: 4
  completed_phases: 3
  total_plans: 8
  completed_plans: 7
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-28)

**Core value:** Evolve btop's architecture toward explicit, testable state machines that eliminate invalid state combinations while preserving every aspect of the user experience.
**Current focus:** Phase 13 - Type-Safe States (fourth phase of v1.1 Automata Architecture)

## Current Position

Phase: 13 of 15 (Type-Safe States)
Plan: 1 of 2 (13-01 complete)
Status: In Progress
Last activity: 2026-02-28 -- Completed 13-01 type-safe state foundation

Progress: [█████-----] 50% (1/2 plans in phase 13)

## Performance Metrics

**Velocity:**
- Total plans completed: 7
- Average duration: 3.4min
- Total execution time: 0.40 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 10-name-states P01 | 1 tasks | 2min | 2min |
| 10-name-states P02 | 2 tasks | 5min | 5min |
| 11-event-queue P01 | 1 tasks | 4min | 4min |
| 11-event-queue P02 | 2 tasks | 4min | 4min |
| 13-type-safe-states P01 | 2 tasks | 5min | 5min |

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

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-28
Stopped at: Completed 13-01-PLAN.md (type-safe state foundation)
Resume file: None
