---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: Automata Architecture
status: unknown
last_updated: "2026-03-01T13:10:37.224Z"
progress:
  total_phases: 6
  completed_phases: 6
  total_plans: 13
  completed_plans: 13
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-28)

**Core value:** Evolve btop's architecture toward explicit, testable state machines that eliminate invalid state combinations while preserving every aspect of the user experience.
**Current focus:** v1.1 Automata Architecture milestone COMPLETE

## Current Position

Phase: 15 of 15 (Verification) -- COMPLETE
Plan: 2 of 2 (all plans complete)
Status: Complete
Last activity: 2026-03-01 -- Completed 15-02 sanitizer sweeps (ASan+UBSan and TSan clean, zero findings)

Progress: [██████████] 100% (2/2 plans in phase 15, 13/13 plans total)

## Performance Metrics

**Velocity:**
- Total plans completed: 13
- Average duration: 3.8min
- Total execution time: 0.82 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 10-name-states P01 | 1 tasks | 2min | 2min |
| 10-name-states P02 | 2 tasks | 5min | 5min |
| 11-event-queue P01 | 1 tasks | 4min | 4min |
| 11-event-queue P02 | 2 tasks | 4min | 4min |
| 13-type-safe-states P01 | 2 tasks | 5min | 5min |
| 13-type-safe-states P02 | 2 tasks | 5min | 5min |
| 13-type-safe-states P03 | 2 tasks | 2min | 2min |
| 14-runner-fsm P01 | 2 tasks | 3min | 3min |
| 14-runner-fsm P02 | 2 tasks | 8min | 8min |
| 15-verification P01 | 2 tasks | 4min | 4min |
| 15-verification P02 | 2 tasks | 4min | 4min |

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
- [Phase 13-type-safe-states]: clean_quit uses static bool re-entrancy guard (shadow atomic conflicts with transition_to ordering)
- [Phase 13-type-safe-states]: Resume from Sleeping routes through Resizing for full redraw via on_enter(Resizing)
- [Phase 14-runner-fsm]: runner:: namespace placed outside Global:: matching state:: pattern
- [Phase 14-runner-fsm]: to_runner_tag uses if-constexpr chain inside std::visit for compile-time exhaustiveness
- [Phase 14-runner-fsm]: Runner::redraw extern kept temporarily (btop_draw.cpp still uses it; Plan 02 migrates)
- [Phase 14-runner-fsm]: Compatibility wrappers (is_stopping, is_active) use acquire ordering on runner_state_tag
- [Phase 14-runner-fsm]: Legacy flags kept as stubs during incremental migration then removed
- [Phase 14-runner-fsm]: wait_idle() polling loop over atomic_wait for multi-state waiting
- [Phase 14-runner-fsm]: pending_redraw folded into conf.force_redraw in Runner::run()
- [Phase 14-runner-fsm]: btop_tools.hpp included in btop_shared.hpp for Tools::atomic_wait in wait_idle()
- [Phase 15-verification]: Runner::stop() test sets app_state to Quitting to safely bypass clean_quit path
- [Phase 15-verification]: Runner::stop() ends at Idle (not Stopping) since cancellation protocol resets the tag
- [Phase 15-verification]: RAII RunnerTagGuard saves/restores Global::runner_state_tag for test isolation
- [Phase 15-verification]: FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER needed for sanitizer builds (Homebrew GTest dylib rpath incompatible)
- [Phase 15-verification]: Zero TSan false positives from EventQueue lock-free pattern (no suppression file needed)
- [Phase 15-verification]: Pre-existing RingBuffer.PushBackOnZeroCapacity (1/279) is out-of-scope; not a v1.1 regression

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-01
Stopped at: Completed 15-02-PLAN.md (sanitizer sweeps -- v1.1 milestone complete)
Resume file: None
