---
phase: 11-event-queue
plan: 02
subsystem: events
tags: [signal-handler, async-signal-safe, event-drain, std-visit]

requires:
  - phase: 11-event-queue
    provides: AppEvent variant and EventQueue class (Plan 01)
  - phase: 10-name-states
    provides: AppState enum with priority ordering
provides:
  - Async-signal-safe signal handlers (push only)
  - Main loop event drain with std::visit dispatch
  - process_signal_event overloads for all 7 event types
affects: [12-extract-transitions, 14-runner-states]

tech-stack:
  added: []
  patterns: [signal-handler-to-queue push pattern, main-loop event drain with early exit on terminal states]

key-files:
  created: []
  modified:
    - src/btop.cpp
    - src/btop_shared.hpp

key-decisions:
  - "Input::interrupt() called in signal handler (not in EventQueue::push) to keep queue generic"
  - "SIGUSR1 remains no-op pass-through (used for pselect interrupt, no event needed)"
  - "Early exit on Quitting/Error in drain loop preserves priority semantics"
  - "process_signal_event(Quit/Sleep) preserve Runner::active check from original handler"

patterns-established:
  - "Signal handlers: push event + Input::interrupt() — nothing else"
  - "Event drain: while(try_pop) { visit(...); early-exit on terminal state }"

requirements-completed: [EVENT-03]

duration: 4min
completed: 2026-02-28
---

# Plan 11-02: Signal Handler Migration + Event Drain Summary

**All signal handlers reduced to lock-free event push + pselect interrupt, with main loop drain via std::visit preserving exact behavioral equivalence**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-28
- **Completed:** 2026-02-28
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Made all signal handlers trivially async-signal-safe (only lock-free push + kill syscall)
- Added 7 process_signal_event overloads preserving exact behavioral semantics of original handlers
- Added event drain loop at top of main loop with early-exit on terminal states
- Fixed pre-existing signal-safety violations (SIGWINCH calling term_resize, SIGINT/SIGTSTP calling clean_quit/_sleep from signal context)

## Task Commits

Each task was committed atomically:

1. **Task 1: Define event_queue instance and add #include** - `5dab0d1` (feat)
2. **Task 2: Migrate signal handlers and add main loop event drain** - `54b78f3` (feat)

## Files Created/Modified
- `src/btop.cpp` - Signal handler migration, event_queue definition, process_signal_event overloads, main loop event drain
- `src/btop_shared.hpp` - Added #include "btop_events.hpp"

## Decisions Made
- Input::interrupt() called in signal handler rather than inside EventQueue::push to keep the queue a generic data structure
- SIGUSR1 handler remains a no-op (used solely for pselect interrupt, no event needed)
- Early exit on Quitting/Error in drain loop preserves the priority semantics from btop_state.hpp
- process_signal_event(Quit) and process_signal_event(Sleep) preserve the Runner::active check from the original handler

## Deviations from Plan
None - plan executed exactly as written

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All signal handlers are now async-signal-safe
- Event queue infrastructure ready for Phase 12 (Extract Transitions) to extend with timer events
- ThreadError event type ready for Phase 14 (Runner States) to use
- Full test suite green (149/150, pre-existing RingBuffer failure unrelated)

---
*Phase: 11-event-queue*
*Completed: 2026-02-28*
