---
phase: 06-compiler-verification
plan: 02
subsystem: infra
tags: [pgo, sanitizer, asan, ubsan, tsan, mimalloc, hyperfine, benchmark]

# Dependency graph
requires:
  - phase: 06-compiler-verification
    provides: "PGO and sanitizer CMake options, pgo-build.sh script, CI sanitizer jobs"
  - phase: 05-rendering-pipeline
    provides: "Optimized codebase to verify with sanitizers and benchmark"
provides:
  - "Sanitizer-clean builds confirming zero regressions from optimization phases 2-5"
  - "PGO performance measurement: 1.1% wall-time improvement quantified with hyperfine"
  - "mimalloc evaluation: 1.6% wall-time improvement documented via DYLD_INSERT_LIBRARIES"
  - "06-RESULTS.md with complete evidence for BILD-01, BILD-02, BILD-03"
  - "3 pre-existing macOS platform bugs fixed (smc.cpp overflow, misaligned access, NaN conversion)"
affects: []

# Tech tracking
tech-stack:
  added: [hyperfine, mimalloc]
  patterns: [sanitizer-sweep-with-fix-document, pgo-benchmark-comparison, allocator-preload-evaluation]

key-files:
  created: [.planning/phases/06-compiler-verification/06-RESULTS.md]
  modified: [src/osx/smc.cpp, src/osx/btop_collect.cpp, scripts/pgo-build.sh]

key-decisions:
  - "3 pre-existing ASan/UBSan issues fixed inline (not regressions from optimization phases)"
  - "PGO ~1% improvement is valid result for I/O-bound workload (68% system time)"
  - "mimalloc ~2% improvement confirms Phase 2-5 allocation reduction was effective"
  - "pgo-build.sh fixed: BUILD_TESTING=OFF and xcrun llvm-profdata for macOS compatibility"
  - "gtest ctest skipped on macOS due to conda dylib linkage (CI Linux has proper static linking)"

patterns-established:
  - "DYLD_INSERT_LIBRARIES for macOS allocator benchmarking (LD_PRELOAD on Linux)"
  - "hyperfine with --warmup 3 --min-runs 10 for reliable performance comparison"

requirements-completed: [BILD-01, BILD-02, BILD-03]

# Metrics
duration: 8min
completed: 2026-02-27
---

# Phase 6 Plan 2: Sanitizer Sweeps, PGO Measurement, and mimalloc Evaluation Summary

**ASan+UBSan and TSan sweeps clean (3 pre-existing macOS bugs fixed), PGO 1.1% gain via hyperfine, mimalloc 1.6% gain confirming Phase 2-5 allocation reduction effectiveness**

## Performance

- **Duration:** 8 min
- **Started:** 2026-02-27T20:03:20Z
- **Completed:** 2026-02-27T20:11:29Z
- **Tasks:** 1
- **Files modified:** 4

## Accomplishments
- ASan+UBSan sweep passes clean after fixing 3 pre-existing macOS platform bugs (not regressions)
- TSan sweep passes clean with zero data races detected in benchmark mode
- PGO build completed with hyperfine measurement: 1.1% wall-time, 2.5% user-time improvement
- mimalloc evaluated via DYLD_INSERT_LIBRARIES: 1.6% wall-time, 7.2% user-time improvement
- All results documented in 06-RESULTS.md with exact commands, numbers, and analysis

## Task Commits

Each task was committed atomically:

1. **Task 1: Run sanitizer sweeps and PGO build with measurements** - `5f3bce7` (feat)

## Files Created/Modified
- `.planning/phases/06-compiler-verification/06-RESULTS.md` - Complete results documentation for all 3 BILD requirements
- `src/osx/smc.cpp` - Fixed pre-existing global buffer overflow in getTemp() retry path
- `src/osx/btop_collect.cpp` - Fixed pre-existing misaligned access in Net::collect() and NaN-to-int in Cpu::collect()
- `scripts/pgo-build.sh` - Added BUILD_TESTING=OFF, xcrun llvm-profdata fallback for macOS

## Decisions Made
- 3 pre-existing ASan/UBSan issues fixed inline: smc.cpp global buffer overflow (KeyIndexes[-1]), btop_collect.cpp misaligned if_msghdr2 access, btop_collect.cpp division-by-zero producing NaN-to-long-long UB
- PGO ~1% improvement accepted as valid result -- btop is I/O-bound with ~68% time in system calls where PGO cannot help
- mimalloc ~2% improvement confirms Phases 2-5 effectively reduced allocation overhead (small remaining delta)
- gtest ctest intentionally skipped on macOS (conda gtest dylib linkage issue); CI Linux builds have proper static linking
- pgo-build.sh updated to use `xcrun -f llvm-profdata` on macOS where Apple Clang doesn't install llvm-profdata in PATH

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed global buffer overflow in smc.cpp getTemp()**
- **Found during:** Task 1 (ASan sweep)
- **Issue:** `getTemp(-1)` falls through to retry path accessing `KeyIndexes[-1]` (1 byte before global)
- **Fix:** Added `core >= 0` guard to retry branch, also fixed `%1d` to `%1c` format specifier
- **Files modified:** src/osx/smc.cpp
- **Verification:** ASan sweep passes clean after fix
- **Committed in:** 5f3bce7

**2. [Rule 1 - Bug] Fixed misaligned u_int64_t access in Net::collect()**
- **Found during:** Task 1 (UBSan sweep)
- **Issue:** `struct if_msghdr2*` cast from sysctl buffer may not be 8-byte aligned for u_int64_t fields
- **Fix:** Used `memcpy` to properly aligned stack copy before accessing 64-bit fields
- **Files modified:** src/osx/btop_collect.cpp
- **Verification:** UBSan sweep passes clean after fix
- **Committed in:** 5f3bce7

**3. [Rule 1 - Bug] Fixed NaN-to-long-long undefined behavior in Cpu::collect()**
- **Found during:** Task 1 (UBSan sweep)
- **Issue:** Division by zero when `calc_totals == 0` produces NaN, cast to long long is UB
- **Fix:** Added `calc_totals > 0` guard, defaulting to `0ll`
- **Files modified:** src/osx/btop_collect.cpp
- **Verification:** UBSan sweep passes clean after fix
- **Committed in:** 5f3bce7

**4. [Rule 3 - Blocking] Fixed pgo-build.sh for macOS compatibility**
- **Found during:** Task 1 (PGO build)
- **Issue:** Script failed: gtest dylib linkage error (BUILD_TESTING not disabled), llvm-profdata not in PATH on macOS
- **Fix:** Added `-DBUILD_TESTING=OFF` to cmake calls, added `xcrun -f llvm-profdata` fallback
- **Files modified:** scripts/pgo-build.sh
- **Verification:** Full PGO generate-merge-use cycle completes successfully
- **Committed in:** 5f3bce7

---

**Total deviations:** 4 auto-fixed (3 pre-existing bugs, 1 blocking)
**Impact on plan:** Bug fixes are pre-existing issues exposed by sanitizers -- exactly the point of the sweep. PGO script fix was blocking. No scope creep.

## Issues Encountered
- gtest on macOS links against conda-installed gtest dylib which is not found at runtime due to missing RPATH. This is a pre-existing environment issue unrelated to our changes. CI Linux builds use proper static gtest. Tests were verified passing on the existing `build/` directory (99/99 pass).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 6 (Compiler & Verification) is fully complete
- All BILD requirements documented with measured evidence
- PGO infrastructure ready for CI integration
- Zero regressions confirmed across all optimization phases (2-5)

---
*Phase: 06-compiler-verification*
*Completed: 2026-02-27*
