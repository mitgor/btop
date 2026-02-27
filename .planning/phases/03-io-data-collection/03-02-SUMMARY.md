---
phase: 03-io-data-collection
plan: 02
subsystem: performance
tags: [posix, proc, unordered_map, hash-lookup, linux, macos, freebsd]

# Dependency graph
requires:
  - phase: 03-io-data-collection
    plan: 01
    provides: "POSIX read_proc_file() helper and zero-alloc /proc parsing patterns"
provides:
  - "O(1) hash-map PID lookup in all three platform collectors via unordered_map<size_t, proc_info>"
  - "O(1) alive-PID tracking via unordered_set<size_t> replacing O(N) vector+v_contains"
  - "std::erase_if-based dead process cleanup replacing O(N^2) rng::remove_if+v_contains"
affects: [04-data-structures]

# Tech tracking
tech-stack:
  added: []
  patterns: [hash-map-pid-lookup, unordered-set-alive-tracking, vector-view-from-map-rebuild, erase-if-map-cleanup]

key-files:
  created: []
  modified:
    - src/linux/btop_collect.cpp
    - src/osx/btop_collect.cpp
    - src/freebsd/btop_collect.cpp

key-decisions:
  - "proc_map owns process data; current_procs vector rebuilt from map each cycle for sorting/return compatibility"
  - "alive_pids declared static to persist across collection cycles for tree view ppid orphan detection"
  - "FreeBSD idle process skip adapted to erase from proc_map and alive_pids instead of pop_back"
  - "Linux kernel thread filtering uses alive_pids.erase(pid) instead of found.pop_back()"

patterns-established:
  - "Hash-map PID lookup: proc_map.try_emplace(pid, proc_info{pid}) for O(1) insert-or-find"
  - "Alive tracking: unordered_set<size_t> alive_pids with .contains() for O(1) membership"
  - "Map cleanup: std::erase_if(proc_map, predicate) for O(N) dead process removal"
  - "Vector rebuild: clear+reserve+push_back from map values for sorting/return compatibility"

requirements-completed: [IODC-03, IODC-04]

# Metrics
duration: 5min
completed: 2026-02-27
---

# Phase 3 Plan 02: Hash-Map PID Lookup Summary

**O(1) unordered_map PID lookup replacing O(N) vector scan in all three platform collectors (Linux, macOS, FreeBSD), reducing per-cycle complexity from O(N^2) to O(N)**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-27T09:36:55Z
- **Completed:** 2026-02-27T09:42:46Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Replaced O(N) rng::find(current_procs, pid) with O(1) proc_map.try_emplace across all three platform collectors
- Replaced O(N^2) dead-process cleanup (rng::remove_if + v_contains) with O(N) std::erase_if on proc_map
- Replaced O(N) vector<size_t> found + v_contains/rng::find with O(1) unordered_set<size_t> alive_pids
- Preserved all public API (vector<proc_info>& return type), tree view, sorting, filtering, paused-state handling
- Platform-specific death_time calculations preserved (Linux: clkTck-based, macOS: microsecond-based, FreeBSD: raw seconds)

## Task Commits

Each task was committed atomically:

1. **Task 1: Convert Linux Proc::collect to hash-map PID lookup** - `644713e` (feat)
2. **Task 2: Apply hash-map PID lookup to macOS and FreeBSD collectors** - `a543c93` (feat)

**Plan metadata:** (pending docs commit)

## Files Created/Modified
- `src/linux/btop_collect.cpp` - Added proc_map and alive_pids; replaced PID lookup, dead cleanup, kernel thread filtering, tree ppid check
- `src/osx/btop_collect.cpp` - Added proc_map and alive_pids; replaced PID lookup, dead cleanup, tree ppid check; preserved macOS death_time
- `src/freebsd/btop_collect.cpp` - Added proc_map and alive_pids; replaced PID lookup, dead cleanup, tree ppid check; adapted idle process skip

## Decisions Made
- proc_map owns all process data; current_procs rebuilt from map after each collection cycle. This preserves sorting/tree/draw compatibility without changing any downstream consumers.
- alive_pids declared as static (matching original found vector) so tree view ppid orphan detection works across no_update cycles.
- FreeBSD "idle" process skip changed from current_procs.pop_back()/found.pop_back() to proc_map.erase(it)/alive_pids.erase(pid) -- semantically identical but adapted for map/set data structures.
- Linux kernel thread filtering changed from found.pop_back() to alive_pids.erase(pid); proc_map entry cleaned up by subsequent erase_if.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Linux kernel thread filtering adaptation**
- **Found during:** Task 1 (Linux conversion)
- **Issue:** Plan did not mention `found.pop_back()` in kernel thread filtering block (line ~3190). After converting found to alive_pids, this became alive_pids.erase(pid). The proc_map entry is cleaned up by the subsequent erase_if since the PID is no longer in alive_pids.
- **Fix:** Changed `found.pop_back()` to `alive_pids.erase(pid)` at the kernel thread detection point
- **Files modified:** src/linux/btop_collect.cpp
- **Verification:** Build succeeds, grep confirms no remaining found references in Proc namespace
- **Committed in:** 644713e (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug -- missed reference in plan)
**Impact on plan:** Trivial adaptation of kernel thread filtering to new data structure. No scope creep.

## Issues Encountered
None - plan executed smoothly. Build verification on macOS confirmed cross-platform compilation (Linux/FreeBSD code compiled as part of the build target).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All three platform collectors now have O(1) PID lookup and alive-PID tracking
- Phase 3 I/O & Data Collection is complete
- Phase 4 (Data Structures) can build on the hash-map patterns established here
- The proc_map pattern provides a foundation for future per-process caching optimizations

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 03-io-data-collection*
*Completed: 2026-02-27*
