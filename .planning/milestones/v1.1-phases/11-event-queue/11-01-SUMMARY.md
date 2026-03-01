---
phase: 11-event-queue
plan: 01
subsystem: events
tags: [variant, lock-free, spsc, ring-buffer, signal-safety, atomic]

requires:
  - phase: 10-name-states
    provides: AppState enum with atomic operations
provides:
  - AppEvent variant with 7 trivially-copyable event types
  - Lock-free SPSC EventQueue template class
  - Comprehensive unit tests for event types and queue operations
affects: [11-event-queue, 12-extract-transitions]

tech-stack:
  added: []
  patterns: [SPSC ring buffer with acquire-release ordering, trivially-copyable event variant]

key-files:
  created:
    - src/btop_events.hpp
    - tests/test_events.cpp
  modified:
    - tests/CMakeLists.txt

key-decisions:
  - "Power-of-2 capacity enforced via C++20 requires clause for bitwise modulo"
  - "Cache-line aligned (alignas(64)) head/tail to prevent false sharing"
  - "TimerTick and ThreadError included in variant now for Phase 12 forward compatibility"
  - "Tests use local EventQueue instances — no dependency on btop.cpp globals"

patterns-established:
  - "Event types as trivially-copyable POD structs in namespace event"
  - "SPSC ring buffer with relaxed load for own index, acquire load for other index, release store for own index"

requirements-completed: [EVENT-01, EVENT-02]

duration: 4min
completed: 2026-02-28
---

# Plan 11-01: Event Types + SPSC EventQueue Summary

**Lock-free SPSC ring buffer and 7-type AppEvent variant with static_assert trivial-copyability guard and 27 unit tests**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-28
- **Completed:** 2026-02-28
- **Tasks:** 1 (TDD)
- **Files modified:** 3

## Accomplishments
- Created AppEvent as std::variant of 7 trivially-copyable event structs (TimerTick, Resize, Quit, Sleep, Resume, Reload, ThreadError)
- Implemented EventQueue template class: lock-free SPSC ring buffer with power-of-2 capacity, cache-line aligned atomics, async-signal-safe push
- 27 unit tests covering all event types, FIFO ordering, overflow detection, empty state, drain-and-refill, and lock-free atomic properties

## Task Commits

Each task was committed atomically:

1. **Task 1: Create btop_events.hpp with event types and SPSC EventQueue** - `f9a53ef` (feat)

## Files Created/Modified
- `src/btop_events.hpp` - Event type definitions (7 structs) + EventQueue template class
- `tests/test_events.cpp` - 27 unit tests for EventType and EventQueue
- `tests/CMakeLists.txt` - Added test_events.cpp to btop_test sources

## Decisions Made
- Power-of-2 capacity enforced via C++20 requires clause for efficient bitwise modulo
- Cache-line aligned head/tail atomics (alignas(64)) to prevent false sharing between signal handler and main thread
- TimerTick and ThreadError included in variant for Phase 12 forward compatibility (not used in Phase 11 signal handlers)
- Tests use local EventQueue instances to avoid link dependency on btop.cpp globals

## Deviations from Plan
None - plan executed exactly as written

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- btop_events.hpp ready for Plan 11-02 to wire signal handlers to the queue
- EventQueue extern declaration in Global namespace ready for definition in btop.cpp
- All tests green (27/27 pass, pre-existing RingBuffer.PushBackOnZeroCapacity failure unrelated)

---
*Phase: 11-event-queue*
*Completed: 2026-02-28*
