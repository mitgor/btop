---
phase: 04-data-structure-modernization
plan: 03
subsystem: data-structures
tags: [enum-indexed-array, config, cpp23, type-safety, performance]

# Dependency graph
requires:
  - phase: 04-02
    provides: "All struct definitions and collectors migrated to enum-indexed RingBuffers"
provides:
  - "Config::bools, ints, strings stored as enum-indexed std::array instead of unordered_map"
  - "BoolKey, IntKey, StringKey enum classes for compile-time safe config access"
  - "All 436 getB/getI/getS/set/flip call sites migrated to enum keys"
  - "String-based overloads retained for menu runtime key resolution"
  - "Config lock/tmp mechanism uses std::array<std::optional<T>, N>"
  - "Config file read/write uses constexpr enum-to-string name tables"
affects: [benchmarks, all-source-files, config-safety]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Enum-indexed config access: Config::getB(BoolKey::tty_mode) replaces Config::getB(\"tty_mode\")"
    - "String-to-enum runtime lookup: bool_key_from_name(), int_key_from_name(), string_key_from_name()"
    - "Dual API: enum overloads for type-safe access, string overloads for menu runtime dispatch"
    - "Key-type query helpers: is_bool_key(), is_int_key(), is_string_key() for menu runtime type detection"
    - "Constexpr name tables: bool_key_names, int_key_names, string_key_names for serialization"

key-files:
  created: []
  modified:
    - "src/btop_config.hpp"
    - "src/btop_config.cpp"
    - "src/btop_draw.cpp"
    - "src/btop_input.cpp"
    - "src/btop.cpp"
    - "src/btop_menu.cpp"
    - "src/btop_theme.cpp"
    - "src/btop_tools.cpp"
    - "src/btop_shared.cpp"
    - "src/linux/btop_collect.cpp"
    - "src/osx/btop_collect.cpp"
    - "src/freebsd/btop_collect.cpp"
    - "src/openbsd/btop_collect.cpp"
    - "src/netbsd/btop_collect.cpp"

key-decisions:
  - "Kept string-accepting overloads for getB/getI/getS/set/flip -- required by menu system which resolves config keys at runtime from categories data structure"
  - "Added is_bool_key/is_int_key/is_string_key helper functions to replace Config::bools.contains()/ints.contains() calls in menu"
  - "Used IIFE (immediately invoked function expression) for array initialization to set non-zero defaults"
  - "Used std::array<std::optional<T>, N> for tmp storage instead of separate map -- allows O(1) merge on unlock"

patterns-established:
  - "Pattern: all fixed-key config maps replaced with enum-indexed std::array -- typos are now compile errors"
  - "Pattern: dual API (enum + string overloads) for config access -- enum for known keys, string for runtime resolution"

requirements-completed: [DATA-01, DATA-03]

# Metrics
duration: 11min
completed: 2026-02-27
---

# Phase 04 Plan 03: Config Enum Migration Summary

**Migrated Config storage from unordered_map<string_view, T> to enum-indexed std::array<T, N> with type-safe BoolKey/IntKey/StringKey enums across 14 source files and 436 call sites**

## Performance

- **Duration:** 11 min
- **Started:** 2026-02-27T16:20:01Z
- **Completed:** 2026-02-27T16:31:00Z
- **Tasks:** 3/3
- **Files modified:** 14

## Accomplishments
- Defined BoolKey (61 entries), IntKey (12 entries), StringKey (30+ entries) enum classes with constexpr name tables for serialization
- Replaced all 6 unordered_map config stores (bools, boolsTmp, ints, intsTmp, strings, stringsTmp) with enum-indexed std::array
- Migrated all 436 Config::getB/getI/getS/set/flip call sites across 12 consumer files from string literals to enum keys
- Config key typos are now compile errors instead of runtime crashes
- All 51 unit tests pass, full build succeeds with 0 errors

## Task Commits

Each task was committed atomically:

1. **Task 1: Define Config enums and migrate Config infrastructure** - `63744e0` (feat)
2. **Task 2: Migrate btop_draw.cpp and btop_input.cpp** - `fbaa8e6` (feat)
3. **Task 3: Migrate remaining 10 files** - `1f87746` (feat)

## Files Created/Modified
- `src/btop_config.hpp` - BoolKey/IntKey/StringKey enums, constexpr name tables, enum-accepting getB/getI/getS/set overloads, string-to-enum lookup functions, is_bool_key/is_int_key/is_string_key helpers
- `src/btop_config.cpp` - Array-based storage initialization via IIFE, enum-based load/write/unlock/flip/getAsString, string-based set/flip overloads delegating to enum versions
- `src/btop_draw.cpp` - 128 Config call sites migrated to enum keys
- `src/btop_input.cpp` - 105 Config call sites migrated to enum keys
- `src/btop.cpp` - 31 Config call sites migrated
- `src/btop_menu.cpp` - 24 Config call sites migrated; Config::ints.contains/bools.contains replaced with is_int_key/is_bool_key
- `src/btop_theme.cpp` - 6 Config call sites migrated
- `src/btop_tools.cpp` - 3 Config call sites migrated
- `src/btop_shared.cpp` - 1 Config call site migrated
- `src/linux/btop_collect.cpp` - 39 Config call sites migrated; Config::ints.at direct access replaced
- `src/osx/btop_collect.cpp` - 27 Config call sites migrated; Config::ints.at direct access replaced
- `src/freebsd/btop_collect.cpp` - 24 Config call sites migrated; Config::ints.at direct access replaced
- `src/openbsd/btop_collect.cpp` - 24 Config call sites migrated; Config::ints.at direct access replaced
- `src/netbsd/btop_collect.cpp` - 24 Config call sites migrated; Config::ints.at direct access replaced

## Decisions Made
- **Kept string-accepting overloads**: The menu system (btop_menu.cpp) resolves config keys dynamically at runtime from a `categories` data structure containing string key names. String-based getB/getI/getS/set/flip overloads delegate to enum lookup internally but are necessary for this use case.
- **Added key-type query helpers**: btop_menu.cpp used `Config::ints.contains()` and `Config::bools.contains()` to detect the type of a runtime config key. Added `is_bool_key()`, `is_int_key()`, `is_string_key()` inline helpers as the array replacement.
- **IIFE for array initialization**: Used immediately-invoked lambda expressions to initialize default values in the std::array storage, since arrays don't support initializer-list key-value syntax.
- **std::optional arrays for tmp storage**: Replaced `unordered_map<string_view, T> Tmp` maps with `std::array<std::optional<T>, N>` -- allows O(1) indexed access and simple `has_value()` checks during unlock merge.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added is_bool_key/is_int_key/is_string_key helpers**
- **Found during:** Task 1 (compiling btop_menu.cpp after storage type change)
- **Issue:** btop_menu.cpp used `Config::ints.contains(selOption)` and `Config::bools.contains(selOption)` for runtime type dispatch, which is not available on std::array.
- **Fix:** Added `is_bool_key()`, `is_int_key()`, `is_string_key()` inline helper functions to btop_config.hpp
- **Files modified:** `src/btop_config.hpp`
- **Verification:** Clean build, menu dispatch works correctly
- **Committed in:** 63744e0 (Task 1 commit)

**2. [Rule 1 - Bug] Replaced Config::ints.at("key") direct array writes in 5 collectors**
- **Found during:** Task 3 (compiling platform collectors)
- **Issue:** All 5 platform collectors used `Config::ints.at("proc_selected") = value` to write directly to the config map, bypassing the `set()` API. With arrays, `.at()` no longer accepts string keys.
- **Fix:** Replaced read patterns with `Config::getI(IntKey::key)` and write patterns with `Config::set(IntKey::key, value)`
- **Files modified:** All 5 platform collector files
- **Verification:** Clean build, all tests pass
- **Committed in:** 1f87746 (Task 3 commit)

---

**Total deviations:** 2 auto-fixed (1 missing critical, 1 bug)
**Impact on plan:** Both auto-fixes necessary for correct compilation. No scope creep.

## Issues Encountered
None - migration was mechanical find-and-replace with straightforward compile-error resolution.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 4 (Data Structure Modernization) is now complete across all 3 plans:
  - Plan 01: RingBuffer template, enums, graph symbol array
  - Plan 02: Struct migration, collector update, pop_front elimination
  - Plan 03: Config enum migration, type-safe access across 14 files
- All fixed-key unordered_maps in the codebase have been replaced with enum-indexed std::array
- Config key typos are compile errors, data structure access is O(1) without hashing
- Ready for Phase 5 or benchmark verification

---
## Self-Check: PASSED

All 14 modified source files verified present. All 3 task commits (63744e0, fbaa8e6, 1f87746) verified in git log.

---
*Phase: 04-data-structure-modernization*
*Completed: 2026-02-27*
