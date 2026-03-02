---
phase: 25-stale-static-const-fix
plan: 01
subsystem: ui
tags: [cpp, static-const, layout, calcSizes, cpu-box]

# Dependency graph
requires: []
provides:
  - "calcSizes() re-evaluates hasCpuHz and freq_range on every call"
  - "CPU box title width updates correctly after config change + resize"
affects: [29-draw-decomposition, 30-redraw-consolidation]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Non-static const for config-dependent locals in functions called on resize/config-change"

key-files:
  created: []
  modified:
    - "src/btop_draw.cpp"

key-decisions:
  - "Remove static from both hasCpuHz and freq_range to match the pattern already used in Cpu::draw()"

patterns-established:
  - "Config-dependent booleans in calcSizes() must not be static -- they must re-evaluate each call"

requirements-completed: [CORR-01]

# Metrics
duration: 1min
completed: 2026-03-02
---

# Phase 25 Plan 01: Stale Static Const Fix Summary

**Removed stale `static const` from calcSizes() hasCpuHz and freq_range so CPU box title width refreshes on config change and resize**

## Performance

- **Duration:** 1 min
- **Started:** 2026-03-02T15:30:54Z
- **Completed:** 2026-03-02T15:32:15Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Fixed stale static const bug in calcSizes() where hasCpuHz and freq_range were evaluated only once on first call
- Both variables now re-evaluate from Cpu::get_cpuHz() and Config::getS(StringKey::freq_mode) on every calcSizes() invocation
- CPU box title width now updates correctly when user changes show_cpu_freq or freq_mode settings and triggers a resize
- All 330 existing tests pass; build succeeds with zero new warnings

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove static from hasCpuHz and freq_range in calcSizes()** - `199f65d` (fix)
2. **Task 2: Build and run all tests** - verification only, no code changes (covered by Task 1 commit)

## Files Created/Modified
- `src/btop_draw.cpp` - Removed `static` from three `const bool` declarations in Draw::calcSizes() CPU box section (lines 2865, 2867, 2869)

## Decisions Made
- Matched the non-static const pattern already established in Cpu::draw() at lines 1340/1342 for freq_range
- Left all other static declarations untouched (clock_custom_format, bat_symbols, battery state tracking) as they are correct for their use cases

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 25 complete; phases 26, 27, and 28 can proceed in parallel (independent subsystems)
- No blockers or concerns

## Self-Check: PASSED

- FOUND: src/btop_draw.cpp
- FOUND: .planning/phases/25-stale-static-const-fix/25-01-SUMMARY.md
- FOUND: commit 199f65d

---
*Phase: 25-stale-static-const-fix*
*Completed: 2026-03-02*
