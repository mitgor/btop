---
phase: 39-platform-io
plan: 01
subsystem: platform
tags: [smc, iokit, iohid, singleton, macos, temperature]

requires:
  - phase: none
    provides: existing SMCConnection and ThermalSensors classes
provides:
  - SMCConnection singleton with automatic reconnect after sleep/wake
  - Cached IOHIDEventSystemClient across getSensors() cycles
  - getGpuTemp() public method on SMCConnection for GPU temperature
affects: [39-02, gpu-monitoring, cpu-monitoring]

tech-stack:
  added: []
  patterns: [singleton-with-reconnect, static-cached-handle]

key-files:
  created: [tests/test_smc_cache.cpp]
  modified: [src/osx/smc.hpp, src/osx/smc.cpp, src/osx/sensors.cpp, src/osx/btop_collect.cpp, tests/CMakeLists.txt]

key-decisions:
  - "SMCConnection singleton via static local (Meyers singleton) — thread-safe init guaranteed by C++11"
  - "Stale connection detection checks kIOReturnNotReady/kIOReturnAborted/kIOReturnNotResponding then reconnects once"
  - "getGpuTemp() added as public method on SMCConnection rather than exposing SMCReadKey — keeps raw API private"
  - "IOHIDEventSystemClient cached as static in getSensors() with lambda initializer — never released"

patterns-established:
  - "Singleton with reconnect: connect()/reconnect() pair for IOKit handles that go stale after sleep/wake"
  - "Static cached CF handles: CFDictionaryRef and IOHIDEventSystemClientRef cached as function-local statics"

requirements-completed: [IO-02]

duration: 5min
completed: 2026-03-14
---

# Phase 39 Plan 01: SMC and IOKit Handle Caching Summary

**SMCConnection singleton with sleep/wake reconnect, cached IOHIDEventSystemClient, and consolidated GPU temp reading via getGpuTemp()**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-14T13:57:28Z
- **Completed:** 2026-03-14T14:02:03Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- SMCConnection converted to singleton — opened once, reused across all collection cycles
- IOHIDEventSystemClient cached as static in getSensors() — eliminates per-cycle create/release
- get_gpu_temp_smc() reduced from 70 lines of inline IOKit code to a single getGpuTemp() call
- Stale connection detection and automatic reconnect after sleep/wake (handles kIOReturnNotReady/kIOReturnAborted/kIOReturnNotResponding)
- 4 new unit tests for singleton identity, connectivity, and temperature reads

## Task Commits

Each task was committed atomically:

1. **Task 1: Convert SMCConnection to singleton with reconnect** - `8182f02` (feat)
2. **Task 2: Cache IOHIDEventSystemClient and update all callers** - `90e6ecf` (feat)

## Files Created/Modified
- `src/osx/smc.hpp` - SMCConnection singleton API: instance(), is_connected(), getGpuTemp(), deleted copy ops
- `src/osx/smc.cpp` - Singleton implementation with connect()/reconnect(), stale detection in getSMCTemp(), getGpuTemp() with flt/sp78 decode
- `src/osx/sensors.cpp` - Static-cached IOHIDEventSystemClient and thermalMatch dictionary in getSensors()
- `src/osx/btop_collect.cpp` - All callers updated to SMCConnection::instance(), get_gpu_temp_smc() delegates to getGpuTemp()
- `tests/test_smc_cache.cpp` - 4 tests: singleton identity, is_connected, getTemp, getGpuTemp
- `tests/CMakeLists.txt` - Added test_smc_cache.cpp to btop_test sources

## Decisions Made
- SMCConnection uses Meyers singleton (static local in instance()) for thread-safe lazy init
- Stale connection detection retries once after reconnect — avoids infinite retry loops
- getGpuTemp() consolidates GPU SMC key reading (Tg0j, Tg0S) with both flt and sp78 decode
- IOHIDEventSystemClient cached as static but CopyServices still called per-cycle since sensor list can change
- Removed try/catch from update_sensors() — singleton never throws, checks is_connected() instead

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Merged btop_collect.cpp caller updates into Task 1**
- **Found during:** Task 1 (SMCConnection singleton conversion)
- **Issue:** Making the SMCConnection constructor private caused compile errors in btop_collect.cpp callers that were scheduled for Task 2
- **Fix:** Updated all callers (get_sensors, update_sensors, get_gpu_temp_smc) in Task 1 commit
- **Files modified:** src/osx/btop_collect.cpp
- **Verification:** Build succeeds, all 351 tests pass
- **Committed in:** 8182f02 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Task 2 reduced to sensors.cpp caching only since btop_collect.cpp callers were necessarily updated in Task 1. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- SMC and IOKit handles are now cached — ready for 39-02 (remaining platform I/O optimizations)
- All 351 tests pass including 4 new SmcCache tests

## Self-Check: PASSED

All 6 files verified present. Both commit hashes (8182f02, 90e6ecf) found in git log. 351/351 tests passing.

---
*Phase: 39-platform-io*
*Completed: 2026-03-14*
