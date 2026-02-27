---
phase: 04-data-structure-modernization
plan: 01
subsystem: data-structures
tags: [ring-buffer, enum-indexing, circular-buffer, graph-symbols, cpp-template]

# Dependency graph
requires:
  - phase: 03-io-data-collection
    provides: "Stable collector code that uses deque<long long> and unordered_map -- foundation for migration"
provides:
  - "RingBuffer<T> template class with full deque-compatible interface"
  - "CpuField, MemField, NetDir, GpuField, SharedGpuField enum types with constexpr name tables"
  - "GraphSymbolType enum with runtime conversion helpers"
  - "Enum-indexed graph_symbols array replacing string-keyed unordered_map"
  - "std::array<vector<string>, 2> Graph::graphs replacing bool-keyed unordered_map"
affects: [04-02-PLAN, 04-03-PLAN]

# Tech tracking
tech-stack:
  added: []
  patterns: [ring-buffer-template, enum-indexed-arrays, constexpr-name-tables]

key-files:
  created:
    - tests/test_ring_buffer.cpp
    - tests/test_enum_arrays.cpp
  modified:
    - src/btop_shared.hpp
    - src/btop_draw.hpp
    - src/btop_draw.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Placed GraphSymbolType enum and helpers in Draw namespace (btop_draw.hpp) for test accessibility"
  - "Used extern const array in Draw namespace for graph_symbols (definition in .cpp, declaration in .hpp)"
  - "Used graph_bg_symbol() helper to simplify 4 identical call sites for background symbol lookup"
  - "Skipped shared_gpu_percent migration per plan guidance -- will be done in Plan 02 alongside collector changes"

patterns-established:
  - "RingBuffer<T> as drop-in deque<long long> replacement with fixed-capacity, zero steady-state allocation"
  - "Enum class with COUNT sentinel for array sizing: static_cast<size_t>(Enum::COUNT)"
  - "Constexpr string_view name tables parallel to enums for serialization/debug"
  - "to_*_type() helpers for runtime string-to-enum conversion at boundary points"

requirements-completed: [DATA-01, DATA-02, DATA-03]

# Metrics
duration: 6min
completed: 2026-02-27
---

# Phase 4 Plan 1: Foundational Types Summary

**RingBuffer<T> circular buffer template, 6 field enums with name tables, and enum-indexed graph_symbols/Graph::graphs replacing unordered_maps**

## Performance

- **Duration:** 6 min
- **Started:** 2026-02-27T15:40:04Z
- **Completed:** 2026-02-27T15:46:40Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- RingBuffer<T> template with heap-allocated backing store, full deque-compatible interface (push_back, pop_front, operator[], random-access iterators, resize, copy/move)
- 6 enum types defined: CpuField(11), MemField(7), NetDir(2), GpuField(3), SharedGpuField(3), GraphSymbolType(6) -- all with constexpr name tables
- graph_symbols migrated from unordered_map<string, vector<string>> to std::array<vector<string>, 6> indexed by GraphSymbolType
- Graph::graphs migrated from unordered_map<bool, vector<string>> to std::array<vector<string>, 2>
- 51 total tests passing (22 RingBuffer + 14 enum array + 15 existing)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create RingBuffer template and unit tests** - `9affeac` (feat)
2. **Task 2: Define field enums and migrate graph_symbols + Graph::graphs** - `d011bc3` (feat)

## Files Created/Modified
- `src/btop_shared.hpp` - RingBuffer<T> template class, 5 field enums with constexpr name tables, added memory/iterator/utility includes
- `src/btop_draw.hpp` - GraphSymbolType enum, to_graph_symbol_type(), graph_bg_symbol() helpers, graph_symbols extern declaration, Graph::graphs std::array migration
- `src/btop_draw.cpp` - graph_symbols array definition in Draw namespace, updated 5 call sites from string-keyed map to enum-indexed array
- `tests/test_ring_buffer.cpp` - 22 unit tests covering all RingBuffer operations
- `tests/test_enum_arrays.cpp` - 14 unit tests for GraphSymbolType enum, graph_symbols array, and Graph default construction
- `tests/CMakeLists.txt` - Added test_ring_buffer.cpp and test_enum_arrays.cpp to btop_test executable

## Decisions Made
- Placed GraphSymbolType enum and helpers in btop_draw.hpp (Draw namespace) rather than keeping them local to the .cpp file, enabling direct unit testing of the enum conversion logic
- Created graph_bg_symbol() helper function to eliminate the repeated pattern of resolving "default" to Config graph_symbol, appending "_up", and extracting index 6 -- used in 4 draw functions
- Skipped shared_gpu_percent type migration per plan's revised Part D guidance -- collector code would break since all access sites need simultaneous migration (Plan 02 scope)
- Used random-access iterators in RingBuffer for std::accumulate compatibility (used by net collectors)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- RingBuffer<T> and all enum types ready for Plan 02 to migrate collector structs (cpu_info, mem_info, net_info, gpu_info)
- graph_symbols migration demonstrates the enum-indexed array pattern that Plan 02 will apply to all collector data
- shared_gpu_percent left in original form for Plan 02 to migrate alongside its access sites

---
*Phase: 04-data-structure-modernization*
*Completed: 2026-02-27*
