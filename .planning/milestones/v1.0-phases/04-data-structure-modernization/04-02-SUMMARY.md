---
phase: 04-data-structure-modernization
plan: 02
subsystem: data-structures
tags: [ring-buffer, enum-indexed-array, cpp20, performance, collectors]

# Dependency graph
requires:
  - phase: 04-01
    provides: "RingBuffer<T> template, CpuField/MemField/NetDir/GpuField/SharedGpuField enums, GraphSymbolType enum, graph_symbols array"
provides:
  - "All time-series structs migrated to enum-indexed std::array<RingBuffer<long long>, N>"
  - "All 5 platform collectors using enum-indexed access"
  - "Graph class accepting RingBuffer parameters"
  - "Pop_front trimming loops eliminated across entire codebase"
  - "Net::graph_max and max_count migrated from string-keyed maps to std::array"
affects: [04-03-verification, benchmarks, draw-code, collector-plugins]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Enum-indexed array access: field[std::to_underlying(EnumType::value)]"
    - "RingBuffer capacity-based sizing: buf.resize(width * 2) replaces pop_front loops"
    - "Runtime string-to-enum resolution: cpu_field_from_name(), mem_field_from_name()"
    - "bsd_time_fields/linux_time_fields constexpr arrays for platform-specific CPU field mapping"
    - "NetDir enum iteration: for (const auto dir_idx : {NetDir::download, NetDir::upload})"

key-files:
  created: []
  modified:
    - "src/btop_shared.hpp"
    - "src/btop_shared.cpp"
    - "src/btop_draw.hpp"
    - "src/btop_draw.cpp"
    - "src/btop_input.cpp"
    - "src/linux/btop_collect.cpp"
    - "src/osx/btop_collect.cpp"
    - "src/freebsd/btop_collect.cpp"
    - "src/openbsd/btop_collect.cpp"
    - "src/netbsd/btop_collect.cpp"

key-decisions:
  - "Used capacity-based resize instead of pop_front trimming to manage ring buffer sizes"
  - "Kept cpu_old as local unordered_map<string, long long> since it is scratch storage not a data structure in the hot path"
  - "Migrated Net::graph_max and max_count from string-keyed unordered_map to std::array<uint64_t, 2> for consistency"
  - "Used bsd_time_fields (4 entries) for BSD/macOS vs linux_time_fields (10 entries) to match platform CPU field counts"

patterns-established:
  - "Pattern: all fixed-key unordered_maps replaced with enum-indexed std::array across all collectors"
  - "Pattern: pop_front trimming eliminated -- ring buffers initialized with capacity via resize()"
  - "Pattern: net dir loops use NetDir enum values instead of string iteration"

requirements-completed: [DATA-01, DATA-02]

# Metrics
duration: ~45min
completed: 2026-02-27
---

# Phase 04 Plan 02: Struct Migration and Collector Update Summary

**Migrated all collector data structures from unordered_map<string, deque> to enum-indexed std::array<RingBuffer> across 10 source files and 5 platform collectors**

## Performance

- **Duration:** ~45 min (across two sessions due to context window)
- **Started:** 2026-02-27
- **Completed:** 2026-02-27
- **Tasks:** 2/2
- **Files modified:** 10

## Accomplishments
- Migrated cpu_info, mem_info, net_info, gpu_info, disk_info, detail_container structs from string-keyed unordered_map + deque to enum-indexed std::array + RingBuffer
- Updated all 5 platform collectors (macOS, Linux, FreeBSD, OpenBSD, NetBSD) to use enum-indexed access patterns
- Eliminated all pop_front trimming loops from collectors -- ring buffer capacity-based sizing replaces manual deque management
- Updated Graph class interface to accept RingBuffer<long long> instead of deque<long long>
- Migrated Net::graph_max and max_count from string-keyed maps to flat std::array<uint64_t, 2>
- All 51 unit tests pass, build succeeds with 0 errors and 0 warnings

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate struct definitions, draw interface, and input handler** - `5e964ff` (feat)
2. **Task 2: Migrate all 5 platform collectors** - `62f303c` (feat)

## Files Created/Modified
- `src/btop_shared.hpp` - Struct definitions: cpu_info, mem_info, net_info, gpu_info, disk_info, detail_container migrated to enum-indexed arrays and RingBuffers; extern graph_max declaration updated
- `src/btop_shared.cpp` - shared_gpu_percent definition updated to std::array<RingBuffer>
- `src/btop_draw.hpp` - Graph class signatures changed from deque to RingBuffer parameters
- `src/btop_draw.cpp` - All safeVal() calls replaced with enum-indexed array access; graph_max access migrated
- `src/btop_input.cpp` - Net stat/bandwidth access migrated to enum-indexed
- `src/linux/btop_collect.cpp` - All collector patterns migrated; linux_time_fields constexpr array added; net dir loops refactored to NetDir enum
- `src/osx/btop_collect.cpp` - All collector patterns migrated; bsd_time_fields constexpr array added; net dir loops refactored
- `src/freebsd/btop_collect.cpp` - All collector patterns migrated; bsd_time_fields added; mem.percent pop_front eliminated; net dir loops refactored
- `src/openbsd/btop_collect.cpp` - Same migrations as FreeBSD
- `src/netbsd/btop_collect.cpp` - Same migrations as FreeBSD

## Decisions Made
- **Kept cpu_old as string-keyed map**: The `cpu_old` unordered_map in each collector stores previous CPU tick values for delta computation. It is local scratch storage, not part of the data structures in the hot render path, so migrating it would add complexity without performance benefit.
- **Used capacity-based resize over bulk init**: Rather than pre-initializing all ring buffer capacities at startup, each collect cycle checks `buf.capacity() != width * 2` and resizes on first use. This avoids needing to know the terminal width at static init time.
- **Migrated graph_max/max_count in Net namespace**: These were `unordered_map<string, uint64_t>` with only "download"/"upload" keys. Migrating to `std::array<uint64_t, 2>` indexed by NetDir provides consistency with the rest of the migration and eliminates 2 more hash map lookups per net collect cycle.
- **bsd_time_fields vs linux_time_fields**: BSD/macOS only reports 4 CPU time fields (user, nice, system, idle) while Linux reports 10. Using separate constexpr arrays avoids out-of-bounds access and documents the platform difference clearly.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed Net::graph_max extern declaration mismatch**
- **Found during:** Task 2 (macOS collector compilation)
- **Issue:** `btop_shared.hpp` declared `extern std::unordered_map<string, uint64_t> graph_max` but collector definitions changed to `std::array<uint64_t, 2>`. The type mismatch caused compilation errors.
- **Fix:** Updated extern declaration in `btop_shared.hpp` and all `graph_max` usages in `btop_draw.cpp` to use enum-indexed array access.
- **Files modified:** `src/btop_shared.hpp`, `src/btop_draw.cpp`
- **Verification:** Clean build with 0 errors
- **Committed in:** 62f303c (Task 2 commit)

**2. [Rule 1 - Bug] Fixed initializer-list to RingBuffer conversion errors in btop_draw.cpp**
- **Found during:** Task 1 (draw code compilation)
- **Issue:** Code like `Graph g(..., {comb_val}, ...)` and `p_graphs.at(p.pid)({(long long)val}, ...)` used initializer lists where RingBuffer was now expected. Initializer lists cannot implicitly convert to RingBuffer.
- **Fix:** Created temporary RingBuffer objects: `RingBuffer<long long> tmp(1); tmp.push_back(val);`
- **Files modified:** `src/btop_draw.cpp`
- **Verification:** Clean compilation
- **Committed in:** 5e964ff (Task 1 commit)

**3. [Rule 2 - Missing Critical] Migrated Net graph_max/max_count in all 5 collectors**
- **Found during:** Task 2 (systematic review)
- **Issue:** Plan specified migrating struct fields but did not mention the `Net::graph_max` and `Net::max_count` static variables which were `unordered_map<string, uint64_t>` and `unordered_map<string, array<int, 2>>` with only "download"/"upload" keys.
- **Fix:** Migrated both to `std::array` indexed by `std::to_underlying(NetDir::*)` across all 5 collectors. Refactored the net dir iteration loops from string-based `for (const string dir : {"download", "upload"})` to enum-based `for (const auto dir_idx : {NetDir::download, NetDir::upload})`.
- **Files modified:** All 5 collector files, `src/btop_shared.hpp`, `src/btop_draw.cpp`
- **Verification:** Clean build, all tests pass
- **Committed in:** 62f303c (Task 2 commit)

---

**Total deviations:** 3 auto-fixed (2 bugs, 1 missing critical)
**Impact on plan:** All auto-fixes necessary for correct compilation and consistency. No scope creep.

## Issues Encountered
- macOS collector was initially marked "complete" in session 1 but the graph_max/max_count migration was missed. Caught during systematic review in session 2 and fixed along with all other collectors.
- BSD collectors referenced `linux_time_fields` (10 entries) from mechanical find-replace but only have 4 CPU time fields. Fixed by defining local `bsd_time_fields` constexpr arrays with 4 entries.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All data structure migrations complete. The codebase now uses enum-indexed arrays and RingBuffers exclusively for time-series data.
- Ready for Plan 03 (verification and cleanup) which will audit for any remaining legacy patterns and run benchmarks.
- The `core_percent` and `temp` fields remain as `vector<RingBuffer<long long>>` / `vector<deque<long long>>` respectively since they are dynamically sized per-core, which is correct.

---
## Self-Check: PASSED

All 11 source/doc files verified present. Both task commits (5e964ff, 62f303c) verified in git log.

---
*Phase: 04-data-structure-modernization*
*Completed: 2026-02-27*
