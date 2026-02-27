---
phase: 06-compiler-verification
plan: 01
subsystem: infra
tags: [pgo, sanitizer, cmake, ci, asan, ubsan, tsan, clang, gcc]

# Dependency graph
requires:
  - phase: 05-rendering-pipeline
    provides: "Optimized codebase ready for compiler-level optimization and verification"
provides:
  - "BTOP_PGO_GENERATE CMake option for instrumented builds"
  - "BTOP_PGO_USE CMake option for profile-guided optimized builds"
  - "BTOP_SANITIZER CMake option for ASan/UBSan/TSan builds"
  - "scripts/pgo-build.sh automated PGO workflow for Clang and GCC"
  - "CI sanitizer sweep jobs (asan-ubsan + tsan) in cmake-linux.yml"
affects: [06-02-PLAN]

# Tech tracking
tech-stack:
  added: [pgo, sanitizers]
  patterns: [cmake-option-with-mutual-exclusivity, ci-matrix-sanitizer-sweep]

key-files:
  created: [scripts/pgo-build.sh]
  modified: [CMakeLists.txt, .github/workflows/cmake-linux.yml]

key-decisions:
  - "PGO and sanitizer options are mutually exclusive with FATAL_ERROR enforcement"
  - "Sanitizer CI uses Clang 19 with libc++ for consistent toolchain"
  - "Benchmark uses 10 cycles under sanitizer (vs 50 normally) due to 2-15x overhead"
  - "fail-fast: false in sanitizer matrix so both asan-ubsan and tsan always run"

patterns-established:
  - "CMake PGO pattern: option declarations near top, flag application after target definitions"
  - "CI sanitizer pattern: matrix with per-step env vars for sanitizer runtime options"

requirements-completed: [BILD-01, BILD-02]

# Metrics
duration: 3min
completed: 2026-02-27
---

# Phase 6 Plan 1: PGO + Sanitizer Build Infrastructure Summary

**PGO generate/use and sanitizer CMake options with automated PGO build script and CI sanitizer sweep jobs for ASan+UBSan and TSan**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-27T19:57:59Z
- **Completed:** 2026-02-27T20:00:57Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Added BTOP_PGO_GENERATE, BTOP_PGO_USE, and BTOP_SANITIZER CMake options with compile+link flags on both libbtop and btop targets
- Created scripts/pgo-build.sh automating full PGO generate-merge-use cycle for both Clang (llvm-profdata) and GCC (.gcda files)
- Added CI sanitizer job with 2-entry matrix (asan-ubsan, tsan) running ctest + benchmark workload under sanitizers
- Enforced mutual exclusivity between PGO options and sanitizer options with clear FATAL_ERROR messages

## Task Commits

Each task was committed atomically:

1. **Task 1: Add PGO and sanitizer CMake options + PGO build script** - `3a45bda` (feat)
2. **Task 2: Add sanitizer sweep CI jobs to cmake-linux.yml** - `8ecafe1` (feat)

## Files Created/Modified
- `CMakeLists.txt` - Added BTOP_PGO_GENERATE, BTOP_PGO_USE, BTOP_SANITIZER options with flag application to both targets
- `scripts/pgo-build.sh` - Automated PGO build script handling Clang and GCC profile workflows
- `.github/workflows/cmake-linux.yml` - Added sanitizer job with asan-ubsan and tsan matrix entries

## Decisions Made
- PGO and sanitizer options are mutually exclusive (FATAL_ERROR enforced) since instrumented/sanitized builds are incompatible
- Sanitizer CI uses Clang 19 matching the existing CI matrix minimum version
- Benchmark under sanitizer uses 10 cycles (not 50) to account for 2-15x runtime overhead
- fail-fast: false ensures both sanitizer configurations run even if one fails
- Sanitizer env options (halt_on_error, detect_leaks, print_stacktrace) set as per-step env blocks for correct GitHub Actions behavior

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added PGO+sanitizer mutual exclusivity**
- **Found during:** Task 1 (CMake options)
- **Issue:** Plan's must_haves stated "PGO and sanitizer options are mutually exclusive" but only showed PGO_GENERATE vs PGO_USE exclusivity in the code block
- **Fix:** Added two additional FATAL_ERROR checks: PGO_GENERATE+SANITIZER and PGO_USE+SANITIZER
- **Files modified:** CMakeLists.txt
- **Verification:** cmake -B /tmp/test -DBTOP_PGO_GENERATE=ON -DBTOP_SANITIZER=address fails with expected error
- **Committed in:** 3a45bda (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Essential correctness guard preventing invalid build configurations. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- PGO and sanitizer CMake options ready for Plan 06-02 to execute sanitizer sweeps, PGO measurement, and mimalloc evaluation
- CI sanitizer jobs will run automatically on next push to main or PR

---
*Phase: 06-compiler-verification*
*Completed: 2026-02-27*
