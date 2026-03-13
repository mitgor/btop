---
phase: 35-build-compiler
plan: 01
subsystem: build
tags: [pgo, cli, training-workload, profile-guided-optimization]

# Dependency graph
requires: []
provides:
  - "--pgo-training CLI flag for comprehensive PGO profile generation"
  - "7-phase training workload exercising sort, filter, tree, resize, box configs"
  - "Updated pgo-build.sh with --pgo-training and LLVM_PROFILE_FILE support"
affects: [35-02, pgo-build]

# Tech tracking
tech-stack:
  added: []
  patterns: ["multi-phase PGO training workload pattern"]

key-files:
  created: []
  modified:
    - src/btop_cli.hpp
    - src/btop_cli.cpp
    - src/btop.cpp
    - scripts/pgo-build.sh

key-decisions:
  - "Used lambdas for collect_draw and draw_only helpers to avoid repetition"
  - "7 phases covering all major code paths: baseline, sort keys, filtering, tree mode, resize, box configs, draw-only"

patterns-established:
  - "PGO training mode as separate early-exit path before benchmark mode"

requirements-completed: [BUILD-01]

# Metrics
duration: 2min
completed: 2026-03-13
---

# Phase 35 Plan 01: PGO Training Workload Summary

**Comprehensive --pgo-training CLI flag exercising sort keys, filtering, tree mode, resize, and box configurations across 49 cycles in 7 phases**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-13T22:06:32Z
- **Completed:** 2026-03-13T22:08:54Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Added --pgo-training CLI flag with comprehensive 7-phase training workload
- Training exercises all major code paths: 5 sort keys, filtering, tree mode, 3 terminal sizes, 4 box configurations, and draw-only cycles
- Updated pgo-build.sh to use --pgo-training with LLVM_PROFILE_FILE for reliable Clang profile collection
- Existing --benchmark mode completely untouched

## Task Commits

Each task was committed atomically:

1. **Task 1: Add --pgo-training CLI flag and comprehensive training workload** - `ca17e81` (feat)
2. **Task 2: Update pgo-build.sh to use --pgo-training** - `624debb` (chore)

## Files Created/Modified
- `src/btop_cli.hpp` - Added pgo_training bool field to Cli struct
- `src/btop_cli.cpp` - Added --pgo-training argument parsing and help text
- `src/btop.cpp` - Comprehensive 7-phase PGO training workload (baseline, sort keys, filtering, tree mode, resize, box configs, draw-only)
- `scripts/pgo-build.sh` - Updated training step to use --pgo-training with LLVM_PROFILE_FILE for Clang

## Decisions Made
- Used lambdas for collect_draw/draw_only helpers to keep training phases DRY
- 7 phases with varying cycle counts (10 baseline, 2 per sort key, 5 filter, 5 tree, 2 per resize, 2 per box config, 5 draw-only = 49 total)
- Moved Clang detection in pgo-build.sh before Step 2 so LLVM_PROFILE_FILE can be set during training

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- PGO training workload complete and verified (exits 0 with all 7 phases)
- pgo-build.sh ready for full PGO pipeline with comprehensive training
- Ready for 35-02 (CMake BTOP_NATIVE and PCH options)

---
*Phase: 35-build-compiler*
*Completed: 2026-03-13*
