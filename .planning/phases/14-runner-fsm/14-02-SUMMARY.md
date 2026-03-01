---
phase: 14-runner-fsm
plan: 02
subsystem: state-machine
tags: [fsm, variant, atomic, runner-thread, migration, cpp20]

# Dependency graph
requires:
  - phase: 14-runner-fsm (plan 01)
    provides: RunnerStateTag enum, runner:: state structs, RunnerStateVar variant, compatibility wrappers, atomic_wait overloads
provides:
  - Fully migrated runner thread with explicit FSM state transitions (Idle/Collecting/Drawing/Stopping)
  - All consumer sites using typed API (is_stopping, is_active, wait_idle, request_redraw)
  - Runner::active, Runner::stopping, Runner::waiting, Runner::redraw removed entirely
  - Runner::pause_output converted to atomic<bool> for TSan safety
  - runner_state_tag as single source of truth for runner thread state
affects: [15-cleanup]

# Tech tracking
tech-stack:
  added: []
  patterns: [runner-fsm-loop, typed-redraw-request, wait-idle-pattern, atomic-pause-output]

key-files:
  created: []
  modified:
    - src/btop.cpp
    - src/btop_shared.hpp
    - src/btop_draw.cpp
    - src/btop_input.cpp
    - src/btop_menu.cpp
    - src/btop_config.cpp
    - src/osx/btop_collect.cpp
    - src/linux/btop_collect.cpp
    - src/freebsd/btop_collect.cpp
    - src/openbsd/btop_collect.cpp
    - src/netbsd/btop_collect.cpp

key-decisions:
  - "Legacy flags kept as stubs during Task 1 for incremental compilation, then removed in Task 2"
  - "Runner::wait_idle() uses polling loop over atomic_wait for multi-state waiting"
  - "pending_redraw folded into conf.force_redraw in Runner::run() before thread_trigger"
  - "btop_tools.hpp included in btop_shared.hpp for Tools::atomic_wait in wait_idle()"

patterns-established:
  - "Runner FSM loop: Idle->Collecting->Drawing->Idle with explicit state transitions"
  - "Consumer sites use typed wrappers (is_stopping, is_active, wait_idle) instead of raw flag reads"
  - "Redraw requests via Runner::request_redraw() folded into runner_conf struct"
  - "Main-to-runner communication: runner_conf (start), RunnerStateTag::Stopping (stop), request_redraw (redraw)"

requirements-completed: [RUNNER-02, RUNNER-03, RUNNER-04]

# Metrics
duration: 8min
completed: 2026-03-01
---

# Phase 14 Plan 02: Runner FSM Migration Summary

**Runner thread migrated to explicit FSM with RunnerStateVar transitions, all 11 consumer files using typed API, old atomic<bool> flags removed**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-01T08:02:45Z
- **Completed:** 2026-03-01T08:11:27Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments
- Runner _runner() loop uses RunnerStateVar with explicit Idle/Collecting/Drawing state transitions and runner_state_tag shadow atomic
- Runner::run(), stop(), on_exit(), clean_quit(), and main loop all migrated to runner_state_tag
- All 5 platform collector files, btop_draw.cpp, btop_input.cpp, btop_menu.cpp, btop_config.cpp migrated from old flags to typed API
- Runner::active, Runner::stopping, Runner::waiting (dead code), Runner::redraw completely removed from both definitions and extern declarations
- Runner::request_redraw() defined with pending_redraw folded into conf.force_redraw by Runner::run()
- Runner::wait_idle() helper added for consumer sites replacing atomic_wait(Runner::active)
- Binary semaphore wake-up pattern (do_work, thread_wait, thread_trigger) preserved unchanged

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate runner loop, run(), stop(), and remove old flags in btop.cpp** - `0eacbea` (feat)
2. **Task 2: Update all consumer sites (collectors, draw, input, menu, config)** - `99c8703` (feat)

## Files Created/Modified
- `src/btop.cpp` - Runner FSM loop with explicit state transitions, run()/stop()/on_exit() migrated, runner_state_tag definition, request_redraw() definition, pending_redraw, atomic pause_output
- `src/btop_shared.hpp` - Removed Runner::active/stopping/redraw externs, added wait_idle(), changed pause_output to atomic<bool>, added btop_tools.hpp include
- `src/btop_draw.cpp` - Runner::is_stopping() (5 sites), Runner::wait_idle() (1 site), Runner::request_redraw() (1 site), pause_output.store() (1 site)
- `src/btop_input.cpp` - Runner::wait_idle() (8 sites)
- `src/btop_menu.cpp` - Runner::wait_idle() (2 sites), pause_output.store() (4 sites)
- `src/btop_config.cpp` - Runner::wait_idle() (1 site)
- `src/osx/btop_collect.cpp` - Runner::is_stopping() (3 sites)
- `src/linux/btop_collect.cpp` - Runner::is_stopping() (6 sites)
- `src/freebsd/btop_collect.cpp` - Runner::is_stopping() (2 sites)
- `src/openbsd/btop_collect.cpp` - Runner::is_stopping() (2 sites)
- `src/netbsd/btop_collect.cpp` - Runner::is_stopping() (2 sites)

## Decisions Made
- Kept legacy flag definitions as stubs during Task 1 to maintain compilation while only btop.cpp was migrated; removed in Task 2 after all consumers migrated (Rule 3 deviation)
- Runner::wait_idle() implemented as a polling loop over atomic_wait(runner_state_tag) since waiting for "not active" requires checking multiple tag values
- pending_redraw is folded into current_conf.force_redraw in Runner::run() before thread_trigger, keeping the runner loop's check simple (just conf.force_redraw)
- Added btop_tools.hpp include to btop_shared.hpp to provide Tools::atomic_wait for the inline wait_idle() function
- Runner::stop() at the end stores Idle and notifies (replacing stopping=false), allowing clean re-entry

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Kept legacy flag stubs during Task 1 for incremental compilation**
- **Found during:** Task 1 (runner loop migration)
- **Issue:** Plan specified removing active/stopping/redraw externs and definitions in Task 1, but consumer files still reference them. Removing would break compilation.
- **Fix:** Kept legacy flag definitions in btop.cpp and extern declarations in btop_shared.hpp during Task 1. Removed both in Task 2 after all consumers were migrated.
- **Files modified:** src/btop.cpp, src/btop_shared.hpp
- **Verification:** Full build succeeds at every commit; all tests pass
- **Committed in:** 0eacbea (Task 1), 99c8703 (Task 2 removal)

**2. [Rule 3 - Blocking] Added btop_tools.hpp include to btop_shared.hpp**
- **Found during:** Task 1 (adding wait_idle() inline function)
- **Issue:** wait_idle() uses Tools::atomic_wait() which is declared in btop_tools.hpp, but btop_shared.hpp did not include it. Compilation failed with "no matching function for call to 'atomic_wait'"
- **Fix:** Added `#include "btop_tools.hpp"` to btop_shared.hpp. No circular dependency (btop_tools.hpp does not include btop_shared.hpp).
- **Files modified:** src/btop_shared.hpp
- **Verification:** Clean compilation of all translation units
- **Committed in:** 0eacbea (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both fixes necessary for correct incremental compilation. No scope creep. Final result matches plan specification exactly.

## Issues Encountered
None beyond the deviations above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Runner FSM migration is complete: runner_state_tag is the single source of truth
- All consumer code uses typed API (is_stopping, is_active, wait_idle, request_redraw)
- Phase 15 cleanup can remove atomic_lock from btop_tools if desired (still used by term_resize, init_config, input polling)
- Binary semaphore pattern preserved unchanged for future optimization if needed

## Self-Check: PASSED

All files exist, all commits verified, SUMMARY.md created.

---
*Phase: 14-runner-fsm*
*Completed: 2026-03-01*
