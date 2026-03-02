---
phase: 24-tests
plan: 02
subsystem: testing
tags: [asan, ubsan, tsan, sanitizers, googletest, memory-safety]

# Dependency graph
requires:
  - phase: 24-tests
    plan: 01
    provides: "PDA invariant tests and Input FSM tests (330 total tests)"
provides:
  - "ASan+UBSan clean build and run (330 tests, zero findings)"
  - "TSan clean build and run (330 tests, zero findings)"
  - "TEST-07 satisfied: sanitizer-clean test suite"
affects: [25-cleanup]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER to force source-built GTest under sanitizers"
    - "Separate build directories for ASan+UBSan vs TSan (mutually exclusive sanitizers)"

key-files:
  created: []
  modified: []

key-decisions:
  - "Force FetchContent source build for GTest under sanitizer builds to avoid conda system GTest dylib mismatch"
  - "No sanitizer suppressions needed -- all 330 tests pass clean under both ASan+UBSan and TSan"

patterns-established:
  - "Sanitizer build command: cmake -B build-asan -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DBTOP_SANITIZER=address,undefined -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER"
  - "TSan build command: cmake -B build-tsan -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DBTOP_SANITIZER=thread -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER"

requirements-completed: [TEST-07]

# Metrics
duration: 2min
completed: 2026-03-02
---

# Phase 24 Plan 02: Sanitizer-Clean Builds Summary

**Full 330-test suite passes under ASan+UBSan and TSan with zero sanitizer findings -- no memory errors, undefined behavior, or data races**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-02T14:01:23Z
- **Completed:** 2026-03-02T14:03:23Z
- **Tasks:** 2
- **Files modified:** 0

## Accomplishments
- ASan+UBSan build compiled and all 330 tests passed with zero address/undefined-behavior findings (7ms runtime)
- TSan build compiled and all 330 tests passed with zero data race findings (21ms runtime)
- Test count verified identical across Debug, ASan, and TSan builds (330 each)
- No sanitizer suppressions required -- entire codebase is sanitizer-clean
- TEST-07 requirement fully satisfied

## Task Commits

No source files were modified by this plan (build-and-verify only). Task commits are not applicable for sanitizer verification runs.

## Files Created/Modified

None -- this plan only builds and runs the existing test suite under sanitizer-instrumented compilers. No source code changes were needed.

## Decisions Made
- Used `FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` to force CMake FetchContent to build GoogleTest v1.17.0 from source rather than linking against the conda system-installed GTest v1.11.0 dynamic library, which caused `dyld` load failures under sanitizer builds
- No sanitizer suppressions needed -- zero findings means no `__attribute__((no_sanitize(...)))` annotations or suppression files required

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed GTest dynamic library load failure in sanitizer builds**
- **Found during:** Task 1 (ASan+UBSan build)
- **Issue:** CMake `FetchContent_Declare` with `FIND_PACKAGE_ARGS` found conda-installed GTest v1.11.0 (`/opt/homebrew/Caskroom/miniconda/base/lib/cmake/GTest`) and linked against its dynamic library (`libgtest_main.1.11.0.dylib`), which was not found at runtime via `@rpath`
- **Fix:** Added `-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` to cmake configuration to force building GTest v1.17.0 from source as static libraries
- **Files modified:** None (CMake invocation flag only)
- **Verification:** Both ASan and TSan builds link against static `libgtest_main.a` and run successfully

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** CMake configuration flag added to build commands. No source code changes. No scope creep.

## Issues Encountered
None beyond the GTest linking issue described in deviations.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All PDA and Input FSM tests verified sanitizer-clean (TEST-07 satisfied)
- Combined with Plan 01 results (TEST-01 through TEST-06), all Phase 24 requirements are met
- Phase 25 cleanup can proceed with confidence: no hidden memory errors, UB, or data races in the PDA/FSM code

## Self-Check: PASSED

- 24-02-SUMMARY.md exists
- ASan build: 330 tests, zero sanitizer findings
- TSan build: 330 tests, zero sanitizer findings
- Test count matches across Debug (330), ASan (330), TSan (330)
- No source files modified (build-and-verify plan)

---
*Phase: 24-tests*
*Completed: 2026-03-02*
