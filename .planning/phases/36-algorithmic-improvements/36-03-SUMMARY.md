---
phase: 36-algorithmic-improvements
plan: 03
subsystem: performance
tags: [soa, cache-optimization, sorting, process-list, nanobench, benchmark]

requires:
  - phase: 36-algorithmic-improvements
    provides: partial_sort in proc_sorter and sort benchmark infrastructure (Plan 01)
provides:
  - SoA key extraction in proc_sorter for numeric keys (pid, threads, mem, cpu_p, cpu_c)
  - Sort benchmark with AoS vs SoA comparison and correctness validation
affects: []

tech-stack:
  added: [SoA-key-extraction, thread_local-vector-reuse]
  patterns: [soa-sort-with-permutation, cache-friendly-numeric-sort]

key-files:
  created: []
  modified:
    - src/btop_shared.cpp
    - benchmarks/bench_sort.cpp

key-decisions:
  - "Used double as unified key type for SoA extraction (integers cast to double for uniform comparison)"
  - "thread_local vectors for SortEntry and sorted_front avoid re-allocation each cycle"
  - "SOA_THRESHOLD=128 validated by benchmark showing SoA wins even at N=64"

patterns-established:
  - "SoA key extraction: extract numeric keys into contiguous array, sort indices, permute results"
  - "Correctness validation: compare top-K as pid sets between SoA and stable_sort"

requirements-completed: [ALG-03]

duration: 3min
completed: 2026-03-14
---

# Phase 36 Plan 03: SoA Key Extraction Summary

**SoA key extraction gives 6.6x speedup over stable_sort and 1.7x over AoS partial_sort at N=1000 K=50 for numeric sort keys**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-14T09:29:36Z
- **Completed:** 2026-03-14T09:32:49Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- SoA key extraction in proc_sorter for 5 numeric sort keys (pid, threads, mem, cpu_p, cpu_c) when N >= 128
- 6.59x speedup over stable_sort and 1.68x over AoS partial_sort at N=1000 K=50
- Correctness validation confirms SoA top-50 matches stable_sort in both ascending and descending order
- String keys (name, cmd, user) and tree mode bypass SoA path entirely

## Task Commits

Each task was committed atomically:

1. **Task 1: SoA key extraction and index sort for numeric keys** - `63a785f` (feat)
2. **Task 2: Extend sort benchmark with SoA comparison** - `3b5faf3` (feat)

**Plan metadata:** (pending) (docs: complete plan)

## Files Created/Modified
- `src/btop_shared.cpp` - SortEntry struct, soa_sort helper, SoA routing in all 10 numeric switch cases
- `benchmarks/bench_sort.cpp` - SoA benchmarks at varying N, AoS-vs-SoA head-to-head, threshold boundary tests, correctness validation

## Decisions Made
- Used double as unified key type for SoA extraction: integers cast to double preserves ordering for all numeric proc_info fields (pid, threads, mem up to 2^53 which covers all practical values)
- thread_local vectors for SortEntry keys and sorted_front avoid heap allocation each sort cycle; only the runner thread calls proc_sorter so no contention
- SOA_THRESHOLD set to 128 but benchmark shows SoA wins even at N=64 (2.57x); threshold is conservative for safety

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Moved SoA code inside Proc namespace**
- **Found during:** Task 1
- **Issue:** SortEntry/soa_sort were in anonymous namespace outside Proc, so proc_info type was not visible (compilation error)
- **Fix:** Moved anonymous namespace containing SortEntry and soa_sort inside Proc namespace
- **Files modified:** src/btop_shared.cpp
- **Verification:** Full release build compiles cleanly
- **Committed in:** 63a785f (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Trivial namespace scoping fix. No scope creep.

## Issues Encountered
None beyond the namespace scoping fix documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All 3 plans in Phase 36 (Algorithmic Improvements) are complete
- Partial sort (Plan 01), constexpr escape sequences (Plan 02), and SoA key extraction (Plan 03) all compose correctly
- SoA composes with partial_sort: at N=1000 K=50, combined benefit is 6.6x over original stable_sort baseline

## Self-Check: PASSED

- src/btop_shared.cpp contains SortEntry, soa_sort, SOA_THRESHOLD
- benchmarks/bench_sort.cpp contains SoA benchmarks and correctness validation
- Commit 63a785f verified
- Commit 3b5faf3 verified

---
*Phase: 36-algorithmic-improvements*
*Completed: 2026-03-14*
