---
phase: 04-data-structure-modernization
plan: 04
subsystem: data-structures
tags: [ringbuffer, bsd, freebsd, openbsd, netbsd, benchmarks, config]

# Dependency graph
requires:
  - phase: 04-data-structure-modernization/plan-02
    provides: "RingBuffer type and Graph interface migration"
  - phase: 04-data-structure-modernization/plan-03
    provides: "Config enum key migration (introduced inline linkage bug)"
provides:
  - "Correct resize-before-push pattern in all 3 BSD collectors"
  - "Working bench_draw.cpp using RingBuffer<long long>"
  - "Fixed Config::set enum overload linkage for static library consumers"
affects: [benchmarks, bsd-platforms, config]

# Tech tracking
tech-stack:
  added: []
  patterns: ["resize-before-push guard pattern for RingBuffer initialization"]

key-files:
  created: []
  modified:
    - src/freebsd/btop_collect.cpp
    - src/openbsd/btop_collect.cpp
    - src/netbsd/btop_collect.cpp
    - benchmarks/bench_draw.cpp
    - src/btop_config.hpp
    - src/btop_config.cpp

key-decisions:
  - "Removed inline keyword from Config::set enum overloads to fix static library symbol export"

patterns-established:
  - "resize-before-push: Always call RingBuffer::resize() before first push_back to avoid silent data loss on capacity-0 buffers"

requirements-completed: [DATA-02]

# Metrics
duration: 5min
completed: 2026-02-27
---

# Phase 4 Plan 04: BSD Ring Buffer Fixes and Benchmark Migration Summary

**Fixed BSD temperature data loss from capacity-0 ring buffers and migrated bench_draw.cpp to RingBuffer<long long> interface**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-27T16:55:48Z
- **Completed:** 2026-02-27T17:01:22Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Fixed silent temperature data loss in FreeBSD/OpenBSD/NetBSD collectors by adding resize(20) guards before push_back
- Fixed core_percent first-data-point drop by swapping resize/push_back ordering in all 3 BSD files
- Migrated bench_draw.cpp from deque<long long> to RingBuffer<long long> for Graph interface compatibility
- Fixed Config::set enum overload linkage bug (inline on .cpp definitions prevented static library symbol export)
- All 51 tests pass, 0 build errors, all 7 draw benchmarks produce timing results

## Task Commits

Each task was committed atomically:

1. **Task 1: Fix BSD temperature and core_percent resize-before-push ordering** - `4c28b07` (fix)
2. **Task 2: Fix bench_draw.cpp to use RingBuffer instead of deque** - `1ecc2af` (fix)

## Files Created/Modified
- `src/freebsd/btop_collect.cpp` - Added resize(20) guards for temp, swapped core_percent resize/push ordering
- `src/openbsd/btop_collect.cpp` - Same fixes as FreeBSD
- `src/netbsd/btop_collect.cpp` - Same fixes as FreeBSD
- `benchmarks/bench_draw.cpp` - Replaced deque with RingBuffer, removed deque include/using
- `src/btop_config.hpp` - Removed inline from Config::set enum overload declarations
- `src/btop_config.cpp` - Removed inline from Config::set enum overload definitions

## Decisions Made
- Removed `inline` keyword from Config::set(BoolKey, bool), Config::set(IntKey, int), Config::set(StringKey, const string&) declarations and definitions. The `inline` on .cpp definitions prevented symbol export when linking btop_bench_draw against libbtop as a static library.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed Config::set inline linkage preventing benchmark compilation**
- **Found during:** Task 2 (bench_draw.cpp migration)
- **Issue:** Config::set enum overloads declared `inline` in btop_config.hpp and defined `inline` in btop_config.cpp caused "undefined symbols" linker error when bench_draw linked against libbtop static library. This was introduced in Plan 04-03 (Config enum migration).
- **Fix:** Removed `inline` keyword from both declarations (header) and definitions (cpp) for set(BoolKey), set(IntKey), set(StringKey)
- **Files modified:** src/btop_config.hpp, src/btop_config.cpp
- **Verification:** build-bench clean build succeeds, all benchmarks run
- **Committed in:** 1ecc2af (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug from prior plan)
**Impact on plan:** Essential fix for benchmark compilation. No scope creep.

## Issues Encountered
None beyond the inline linkage issue documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All Phase 4 data structure modernization work complete including gap closure
- Plan 04-05 (measurement evidence) is the remaining plan in Phase 4
- BSD collectors, benchmarks, and Config all working correctly

## Self-Check: PASSED

All 6 modified files verified present. Both task commits (4c28b07, 1ecc2af) verified in git log. SUMMARY.md created successfully.

---
*Phase: 04-data-structure-modernization*
*Completed: 2026-02-27*
