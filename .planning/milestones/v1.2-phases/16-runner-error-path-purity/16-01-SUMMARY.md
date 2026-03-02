---
phase: 16-runner-error-path-purity
plan: 01
subsystem: state-machine
tags: [fsm, event-driven, error-handling, thread-safety, dead-code-removal]

# Dependency graph
requires:
  - phase: 14-event-driven-signals
    provides: "Event queue and dispatch_event infrastructure"
  - phase: 15-app-fsm-states
    provides: "AppStateVar, on_event overloads, transition_to, on_enter/on_exit"
provides:
  - "Pure event-driven runner error paths (no direct shadow atomic writes)"
  - "on_exit(Running, Error) handler for runner thread stop on error"
  - "Removed RunnerStateVar, runner:: structs, to_runner_tag() dead code"
affects: [17-main-loop-fsm-drain, 18-hygiene, 19-measurement]

# Tech tracking
tech-stack:
  added: []
  patterns: ["sideband message + event push pattern for non-trivially-copyable data across event queue"]

key-files:
  created: []
  modified:
    - src/btop.cpp
    - src/btop_state.hpp
    - tests/test_transitions.cpp
    - tests/test_runner_state.cpp

key-decisions:
  - "Used sideband Global::exit_error_msg + event::ThreadError{} pattern because AppEvent must be trivially copyable"
  - "Added on_exit(Running, Error) to stop runner thread instead of doing it inline in catch block"
  - "Kept test_runner_state.cpp with RunnerStateTag enum tests, removed only runner variant/struct tests"

patterns-established:
  - "Sideband pattern: write data to global, push empty event, handler reads global on main thread"
  - "on_exit(OldState, NewState) used for cross-concern cleanup (stopping runner on error transition)"

requirements-completed: [PURE-01, PURE-02, PURE-03, PURE-04]

# Metrics
duration: 7min
completed: 2026-03-01
---

# Phase 16 Plan 01: Runner Error Path Purity Summary

**Event-driven runner error paths via ThreadError event queue push, with runner_var dead code and unused RunnerStateVar type scaffolding removed**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-01T21:03:04Z
- **Completed:** 2026-03-01T21:10:11Z
- **Tasks:** 3
- **Files modified:** 4

## Accomplishments
- Runner thread error paths now push event::ThreadError into event queue instead of directly writing Global::app_state shadow atomic
- on_event(Running, ThreadError) reads actual error message from Global::exit_error_msg sideband, making the handler reachable at runtime
- Removed write-only runner_var local variable and all unused runner:: namespace type infrastructure (RunnerStateVar, to_runner_tag)
- All tests pass under normal, ASan+UBSan, and TSan configurations with zero sanitizer findings

## Task Commits

Each task was committed atomically:

1. **Task 1: Route runner error paths through event queue** - `20e2ddd` (feat)
2. **Task 2: Remove runner_var write-only dead code** - `381353a` (refactor)
3. **Task 3: Verify error path purity with sanitizer sweeps** - `a29e002` (test)

## Files Created/Modified
- `src/btop.cpp` - Updated on_event(Running, ThreadError) to use exit_error_msg sideband; replaced 2 direct app_state.store(Error) with event_queue.push(ThreadError); added on_exit(Running, Error); removed 4 runner_var assignments
- `src/btop_state.hpp` - Removed runner:: namespace (Idle, Collecting, Drawing, Stopping structs), RunnerStateVar typedef, and to_runner_tag() function
- `tests/test_transitions.cpp` - Updated RunningThreadErrorReturnsError test for sideband pattern; added ThreadErrorSyncsVariantAndShadow test
- `tests/test_runner_state.cpp` - Removed 14 tests for deleted runner:: structs, RunnerStateVar, and to_runner_tag; retained RunnerStateTag enum and RunnerFsmQuery tests

## Decisions Made
- Used sideband Global::exit_error_msg + empty event::ThreadError{} pattern because AppEvent must remain trivially copyable (std::string cannot be in the variant)
- Added on_exit(Running, Error) handler to stop runner thread, following the existing pattern of on_exit(Running, Quitting) and on_exit(Running, Sleeping)
- Kept test_runner_state.cpp in the build with RunnerStateTag enum tests intact; only removed tests for deleted types

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Error paths are pure event-driven: runner pushes events, main loop dispatches and transitions
- All runner_var dead code removed, reducing cognitive load for Phase 17 (main loop FSM drain)
- RunnerStateTag enum and runner_state_tag atomic retained for cross-thread state queries
- Pre-existing RingBuffer.PushBackOnZeroCapacity test failure is unrelated (out of scope)

## Self-Check: PASSED

- SUMMARY.md: FOUND
- Commit 20e2ddd (Task 1): FOUND
- Commit 381353a (Task 2): FOUND
- Commit a29e002 (Task 3): FOUND
- src/btop.cpp: FOUND
- src/btop_state.hpp: FOUND
- tests/test_transitions.cpp: FOUND
- tests/test_runner_state.cpp: FOUND

---
*Phase: 16-runner-error-path-purity*
*Completed: 2026-03-01*
