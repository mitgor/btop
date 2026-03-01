---
phase: 10-name-states
plan: 02
subsystem: state-management
tags: [enum, atomic, thread-safety, cpp20, state-machine, refactoring]

# Dependency graph
requires:
  - phase: 10-01
    provides: "AppState enum class in btop_state.hpp with extern atomic declaration"
provides:
  - "Single atomic<AppState> replaces 6 scattered atomic<bool> flags across 4 source files"
  - "Priority-ordered if/else if main loop using enum comparison"
  - "Race-safe state clearing via compare_exchange_strong"
  - "Release/acquire ordering for Error state transitions from runner thread"
affects: [11-transition-rules, 12-extract-transitions, 14-runner-fsm]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "compare_exchange_strong for safe state clearing (prevents swallowing higher-priority signals)"
    - "memory_order_release on Error store / memory_order_acquire on main loop load"
    - "using Global::AppState at file scope for convenience in btop.cpp and btop_draw.cpp"

key-files:
  created: []
  modified:
    - src/btop.cpp
    - src/btop_shared.hpp
    - src/btop_draw.cpp
    - src/btop_tools.cpp

key-decisions:
  - "init_conf and _runner_started remain as atomic<bool> (lock and lifecycle marker, not app states)"
  - "compare_exchange_strong used when clearing state back to Running to prevent race conditions with signal handlers"
  - "Error state stores use memory_order_release paired with acquire in main loop for message visibility"

patterns-established:
  - "State check pattern: load state once per loop iteration, compare against enum values"
  - "State clearing pattern: compare_exchange_strong(expected, Running) instead of plain store"
  - "Cross-thread error pattern: set message BEFORE store(Error, release)"

requirements-completed: [STATE-01, STATE-04]

# Metrics
duration: 5min
completed: 2026-02-28
---

# Phase 10 Plan 02: Flag Migration Summary

**Replaced 6 atomic<bool> flags with single atomic<AppState> across btop.cpp, btop_shared.hpp, btop_draw.cpp, and btop_tools.cpp with race-safe compare_exchange transitions**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-28T09:50:28Z
- **Completed:** 2026-02-28T09:55:14Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Replaced 6 scattered atomic<bool> flags (resized, quitting, should_quit, should_sleep, reload_conf, thread_exception) with single atomic<AppState> in btop.cpp
- Migrated all flag usages across 4 files: signal handlers, main loop, Runner thread loop, clean_quit, term_resize, draw code, and tools
- Used compare_exchange_strong for all state-clearing transitions to prevent race conditions where a signal handler sets a higher-priority state during transition
- Applied memory_order_release/acquire for Error state transitions to guarantee exit_error_msg visibility

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate btop.cpp flag definitions and btop_shared.hpp extern declarations** - `b38b568` (feat)
2. **Task 2: Migrate btop_draw.cpp and btop_tools.cpp flag usages** - `7df0b56` (feat)

## Files Created/Modified
- `src/btop.cpp` - AppState variable definition, migrated main loop, signal handlers, clean_quit, term_resize, Runner thread loop, Runner::stop, Runner::run
- `src/btop_shared.hpp` - Includes btop_state.hpp, removed old flag extern declarations (quitting, resized, thread_exception)
- `src/btop_draw.cpp` - Reads AppState::Resizing instead of Global::resized in clock and calcSizes
- `src/btop_tools.cpp` - Uses compare_exchange_strong to clear Resizing state in Term::refresh

## Decisions Made
- Kept init_conf as atomic<bool> because it serves as an atomic_lock guard, not an app state
- Kept _runner_started as atomic<bool> because it is a one-shot lifecycle marker set once after pthread_create
- Used compare_exchange_strong (not plain store) when clearing Resizing back to Running, preventing the main loop from accidentally swallowing a Quitting signal set by a signal handler during transition
- Applied memory_order_release to Error state stores and memory_order_acquire to the main loop load, ensuring exit_error_msg is visible when the main loop sees Error

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 10 (Name States) is now complete: AppState enum defined, tested, and integrated across codebase
- Phase 11 (Transition Rules) can build on this foundation to add formal state transition validation
- Phase 12 (Extract Transitions) can restructure the main loop now that it uses enum comparison
- Phase 14 (Runner FSM) can apply the same pattern to Runner's own atomic<bool> flags

## Self-Check: PASSED

- All 4 modified files exist on disk
- Both task commits (b38b568, 7df0b56) found in git log
- Build succeeds with zero errors
- 122/123 tests pass (1 pre-existing RingBuffer failure unrelated)
- Zero references to old flag names across entire codebase

---
*Phase: 10-name-states*
*Completed: 2026-02-28*
