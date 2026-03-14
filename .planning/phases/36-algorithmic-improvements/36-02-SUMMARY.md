---
phase: 36-algorithmic-improvements
plan: 02
subsystem: core
tags: [constexpr, string_view, escape-sequences, lookup-tables, compile-time]

# Dependency graph
requires:
  - phase: 36-algorithmic-improvements
    provides: partial sort infrastructure (36-01)
provides:
  - constexpr Fx/Mv/Term escape sequences (inline constexpr const char*)
  - constexpr Key_escapes array with linear-scan lookup
  - constexpr sort_vector and proc_state_name switch function
  - constexpr Config validation arrays (7 arrays migrated)
  - generic v_contains/v_index accepting any input_range
affects: [37-allocation-parsing, 38-output-pipeline]

# Tech tracking
tech-stack:
  added: []
  patterns: [inline-constexpr-const-char-ptr, constexpr-array-string_view, switch-based-lookup]

key-files:
  created: []
  modified:
    - src/btop_tools.hpp
    - src/btop_input.cpp
    - src/btop_shared.hpp
    - src/btop_config.hpp
    - src/btop_menu.cpp
    - src/btop_theme.cpp
    - src/btop_draw.cpp
    - src/linux/btop_collect.cpp

key-decisions:
  - "Used inline constexpr const char* for Fx/Mv/Term instead of string_view to preserve seamless string + operator compatibility across 215+ usage sites"
  - "Used linear-scan for Key_escapes lookup (34 entries at human input rate) instead of binary search to avoid sort-order bugs"
  - "Kept valid_boxes as runtime vector due to conditional #ifdef GPU_SUPPORT entries"
  - "Created static vector<string> copies in btop_menu.cpp for menu optionsList compatibility with constexpr arrays"

patterns-established:
  - "constexpr escape sequences: use const char* (not string_view) for string concatenation compatibility"
  - "constexpr lookup tables: use array of pair<string_view, string_view> with linear scan for small tables"
  - "generic range functions: v_contains/v_index accept any input_range, not just vector"

requirements-completed: [ALG-02]

# Metrics
duration: 8min
completed: 2026-03-14
---

# Phase 36 Plan 02: Constexpr Migration Summary

**Migrated ~60 runtime-initialized const strings/maps to constexpr: Fx/Mv/Term escape codes, Key_escapes, sort_vector, proc_states, and 7 Config validation arrays**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-14T09:16:48Z
- **Completed:** 2026-03-14T09:25:00Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Eliminated runtime initialization of 28 Fx/Mv/Term escape sequence strings (now inline constexpr const char*)
- Replaced Key_escapes unordered_map with constexpr array + O(n) linear-scan lookup function
- Replaced proc_states unordered_map with constexpr switch-based proc_state_name() function
- Converted sort_vector from const vector<string> to constexpr array<string_view, 8>
- Converted 7 Config validation arrays to constexpr array<string_view, N>
- Generalized v_contains/v_index to work with any std::ranges::input_range (not just vector)

## Task Commits

Each task was committed atomically:

1. **Task 1: Constexpr Fx/Mv/Term escape sequences** - `9d6f308` (feat)
2. **Task 2: Constexpr Key_escapes, sort_vector, proc_states, Config validation** - `26a6a08` (feat)

## Files Created/Modified
- `src/btop_tools.hpp` - Constexpr Fx/Mv/Term escape sequences + generic v_contains/v_index
- `src/btop_input.cpp` - Constexpr Key_escapes array + lookup_key_escape() function
- `src/btop_shared.hpp` - Constexpr sort_vector + proc_state_name() switch function
- `src/btop_config.hpp` - 7 constexpr validation arrays (valid_graph_symbols, temp_scales, etc.)
- `src/btop_menu.cpp` - Static vector<string> copies for menu optionsList compatibility
- `src/btop_theme.cpp` - Fixed 2 const char* concatenation sites in hex_to_color/dec_to_color
- `src/btop_draw.cpp` - Fixed 2 const char* concatenation sites in TextEdit and core display
- `src/linux/btop_collect.cpp` - Updated proc_states map usage to proc_state_name() call

## Decisions Made
- Used `inline constexpr const char*` instead of `string_view` for Fx/Mv/Term escape sequences because `string_view` doesn't support `operator+` with `std::string`, which would have required changes at all 215+ usage sites. `const char*` works seamlessly with existing `string + Fx::b` concatenation patterns.
- Used linear scan instead of binary search for Key_escapes because at 34 entries queried once per keypress at human speed, sort-order correctness is more important than theoretical performance.
- Kept `valid_boxes` as runtime `const vector<string>` because it has conditional `#ifdef GPU_SUPPORT` entries that prevent compile-time array sizing.
- Created static `vector<string>` copies in btop_menu.cpp because the menu's `optionsList` uses `reference_wrapper<const vector<string>>` type-homogeneously and mixing constexpr arrays with runtime vectors in the same map type requires adapters.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed const char* + const char* concatenation errors**
- **Found during:** Task 1 (Fx/Mv/Term constexpr migration)
- **Issue:** Plan specified `constexpr string_view` but `string_view + string` concatenation doesn't compile in C++. Switched to `const char*` which works with `string + const_char_ptr`. Then 4 sites using `Fx::X + "literal"` or `Fx::X + Fx::Y` failed because `const char* + const char*` is invalid (pointer arithmetic).
- **Fix:** Wrapped initiating operand with `string()` constructor at 4 sites: btop_theme.cpp (2), btop_draw.cpp (2)
- **Files modified:** src/btop_theme.cpp, src/btop_draw.cpp
- **Verification:** Full build with zero errors and zero warnings
- **Committed in:** 9d6f308 (Task 1 commit)

**2. [Rule 3 - Blocking] Added menu compatibility adapters for constexpr arrays**
- **Found during:** Task 2 (Config validation array migration)
- **Issue:** btop_menu.cpp's `optionsList` stores `reference_wrapper<const vector<string>>` but constexpr arrays are `array<string_view, N>`, type-incompatible
- **Fix:** Created static `vector<string>` copies from constexpr arrays for menu use
- **Files modified:** src/btop_menu.cpp
- **Verification:** Full build with zero errors
- **Committed in:** 26a6a08 (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking)
**Impact on plan:** Both fixes necessary for compilation. No scope creep.

## Issues Encountered
- Test binary has pre-existing linker issue with shared gtest library (dylib not found). Build-test compiles successfully with zero errors/warnings but test binary cannot run. This is not caused by this plan's changes.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Constexpr migration complete, ready for 36-03 (SoA key extraction)
- All v_contains/v_index callers work with both vector and array types

---
*Phase: 36-algorithmic-improvements*
*Completed: 2026-03-14*
