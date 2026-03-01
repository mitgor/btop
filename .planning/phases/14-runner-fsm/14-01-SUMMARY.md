---
phase: 14-runner-fsm
plan: 01
subsystem: state-machine
tags: [fsm, variant, atomic, runner-thread, cpp20]

# Dependency graph
requires:
  - phase: 13-type-safe-states
    provides: AppStateTag/AppStateVar pattern, btop_state.hpp structure, test infrastructure
provides:
  - RunnerStateTag enum (Idle, Collecting, Drawing, Stopping) in btop_state.hpp
  - runner:: state structs (Idle, Collecting, Drawing, Stopping) in btop_state.hpp
  - RunnerStateVar variant type alias in btop_state.hpp
  - to_runner_tag() variant-to-tag converter in btop_state.hpp
  - runner_state_tag extern atomic in Global namespace
  - Runner::is_stopping() and Runner::is_active() compatibility wrappers
  - Runner::request_redraw() declaration
  - atomic_wait/atomic_wait_for overloads for RunnerStateTag
  - 29 unit tests for runner state types
affects: [14-02-runner-fsm-migration]

# Tech tracking
tech-stack:
  added: []
  patterns: [runner-state-tag-enum, runner-variant-structs, compatibility-wrapper-functions]

key-files:
  created:
    - tests/test_runner_state.cpp
  modified:
    - src/btop_state.hpp
    - src/btop_shared.hpp
    - src/btop_tools.hpp
    - src/btop_tools.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Runner::redraw extern kept (still used in btop_draw.cpp) -- Plan 02 migrates usage"
  - "runner:: namespace placed outside Global:: matching state:: pattern from Phase 13"
  - "to_runner_tag uses if-constexpr chain inside std::visit for compile-time exhaustiveness"
  - "atomic_wait overloads follow identical pattern to existing bool versions"

patterns-established:
  - "Runner FSM types mirror App FSM types: tag enum + state structs + variant + to_tag"
  - "Compatibility wrappers (is_stopping, is_active) provide semantic API over raw atomic reads"

requirements-completed: [RUNNER-01, RUNNER-02]

# Metrics
duration: 3min
completed: 2026-03-01
---

# Phase 14 Plan 01: Runner FSM Type System Summary

**RunnerStateTag enum, runner:: state structs, RunnerStateVar variant, compatibility wrappers, and atomic_wait overloads for runner thread FSM**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-01T07:55:24Z
- **Completed:** 2026-03-01T07:59:19Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Defined RunnerStateTag enum with 4 states (Idle, Collecting, Drawing, Stopping) as uint8_t in btop_state.hpp
- Created runner:: state structs with RunnerStateVar variant and to_runner_tag() converter
- Added Runner::is_stopping(), Runner::is_active() compatibility wrappers and request_redraw() declaration
- Added atomic_wait/atomic_wait_for overloads for RunnerStateTag in btop_tools
- Removed dead Runner::reading extern declaration
- All 29 new RunnerState tests pass; full test suite green (239 tests, only pre-existing RingBuffer failure)

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): Add failing tests** - `4c43c62` (test)
2. **Task 1 (GREEN): Define RunnerStateTag, runner:: structs, RunnerStateVar** - `8e26b8b` (feat)
3. **Task 2: Compatibility wrappers, atomic_wait overloads, remove dead code** - `16ffdc9` (feat)

_Note: Task 1 followed TDD pattern with RED/GREEN commits._

## Files Created/Modified
- `src/btop_state.hpp` - RunnerStateTag enum, runner:: state structs, RunnerStateVar variant, to_runner_tag()
- `src/btop_shared.hpp` - Runner::is_stopping(), Runner::is_active(), Runner::request_redraw() declaration; removed Runner::reading
- `src/btop_tools.hpp` - atomic_wait/atomic_wait_for overloads for RunnerStateTag
- `src/btop_tools.cpp` - Implementation of RunnerStateTag atomic_wait/atomic_wait_for
- `tests/test_runner_state.cpp` - 29 unit tests for runner state types
- `tests/CMakeLists.txt` - Added test_runner_state.cpp to test executable

## Decisions Made
- Kept Runner::redraw extern declaration (btop_draw.cpp still writes to it; Plan 02 migrates the usage to request_redraw())
- runner:: namespace placed at file scope (outside Global::) matching existing state:: pattern from Phase 13
- to_runner_tag() uses if-constexpr chain inside std::visit, same pattern as to_tag() for AppStateVar
- atomic_wait overloads use identical implementation to existing bool versions (C++20 atom.wait + sleep loop)
- Added btop_state.hpp include to btop_tools.hpp for RunnerStateTag type visibility

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Kept Runner::redraw extern instead of removing it**
- **Found during:** Task 2 (compatibility wrappers)
- **Issue:** Plan specified removing `extern atomic<bool> redraw` from btop_shared.hpp, but btop_draw.cpp line 2763 still writes `Runner::redraw = true`. Removing the extern would break compilation.
- **Fix:** Kept the extern declaration. Plan 02 will migrate the btop_draw.cpp usage to Runner::request_redraw() and then remove both the extern and definition.
- **Files modified:** src/btop_shared.hpp (kept redraw, removed reading only)
- **Verification:** Full binary builds; all tests pass
- **Committed in:** 16ffdc9 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Minimal -- Runner::redraw retained temporarily to avoid build breakage. Plan 02 handles the migration.

## Issues Encountered
None beyond the deviation above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All runner FSM types are defined and tested, ready for Plan 02 migration
- Compatibility wrappers (is_stopping, is_active) ready for callers to adopt
- request_redraw() declared; Plan 02 provides definition and migrates btop_draw.cpp usage
- Runner::active, Runner::stopping, Runner::redraw kept for Plan 02 to migrate incrementally

## Self-Check: PASSED

All files exist, all commits verified, SUMMARY.md created.

---
*Phase: 14-runner-fsm*
*Completed: 2026-03-01*
