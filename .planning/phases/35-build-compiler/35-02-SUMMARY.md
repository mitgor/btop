---
phase: 35-build-compiler
plan: 02
subsystem: infra
tags: [cmake, march-native, precompiled-headers, build-optimization]

requires:
  - phase: 35-build-compiler/01
    provides: PGO and sanitizer CMake options structure
provides:
  - BTOP_NATIVE CMake option for -march=native CPU optimization
  - BTOP_PCH CMake option for precompiled STL/fmt headers
affects: [build-compiler, README documentation]

tech-stack:
  added: []
  patterns: [cmake-option-declare-early-apply-late, pch-stl-only]

key-files:
  created: []
  modified: [CMakeLists.txt]

key-decisions:
  - "PCH includes only STL and fmt headers, never project headers, to avoid invalidation during development"
  - "Options declared early near other options but applied after CheckCXXCompilerFlag and target definitions"
  - "BTOP_NATIVE gracefully degrades if compiler lacks -march=native support"

patterns-established:
  - "CMake option pattern: declare option() early, apply target_compile_options late after targets and includes exist"

requirements-completed: [BUILD-02, BUILD-03]

duration: 2min
completed: 2026-03-13
---

# Phase 35 Plan 02: Native CPU and PCH CMake Options Summary

**BTOP_NATIVE option adds -march=native to both targets; BTOP_PCH enables precompiled STL/fmt headers for faster clean builds**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-13T22:06:09Z
- **Completed:** 2026-03-13T22:08:31Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Added BTOP_NATIVE=ON option that applies -march=native to both btop and libbtop targets
- Added BTOP_PCH=ON option that precompiles 16 STL and fmt headers for libbtop
- Both options OFF by default with zero impact on default builds
- BTOP_NATIVE includes compiler capability check with graceful fallback

## Task Commits

Each task was committed atomically:

1. **Task 1: Add BTOP_NATIVE and BTOP_PCH CMake options** - `080e963` (feat)
2. **Task 2: Verify default build unaffected and test suite passes** - verification only, no file changes

## Files Created/Modified
- `CMakeLists.txt` - Added BTOP_NATIVE and BTOP_PCH option declarations and conditional application blocks

## Decisions Made
- PCH list restricted to STL and fmt headers only (no project headers) to prevent PCH invalidation during development
- Options declared early (near other options around line 60) but applied late (after targets defined and CheckCXXCompilerFlag included)
- BTOP_NATIVE checks compiler support via check_cxx_compiler_flag and sets itself OFF with a warning if unsupported

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- Pre-existing gtest rpath issue causes test discovery to fail during cmake build (dyld cannot find libgtest_main), but tests run successfully when executed directly with DYLD_LIBRARY_PATH set. All 336 tests pass. This is not caused by the current changes.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- CMakeLists.txt now has full build optimization support: LTO, PGO, sanitizers, native CPU, and PCH
- Ready for Phase 36 or further build infrastructure work

## Self-Check: PASSED

- CMakeLists.txt: FOUND
- Commit 080e963: FOUND

---
*Phase: 35-build-compiler*
*Completed: 2026-03-13*
