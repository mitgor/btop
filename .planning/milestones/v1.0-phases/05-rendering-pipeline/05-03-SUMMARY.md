---
phase: 05-rendering-pipeline
plan: 03
subsystem: rendering
tags: [terminal, escape-sequences, double-buffering, differential-rendering, cell-buffer]

# Dependency graph
requires:
  - phase: 05-01
    provides: Tools::write_stdout for frame output
  - phase: 05-02
    provides: Graph last_data_back caching (data_same optimization)
provides:
  - Cell struct with UTF-8 char, truecolor/256-color, attribute bitfield
  - ScreenBuffer double-buffered cell buffer with resize/swap/clear
  - render_to_buffer escape-string-to-cell-buffer parser (CSI cursor, SGR color/attr, UTF-8)
  - diff_and_emit differential output emitting only changed cells
  - full_emit force-redraw complete screen write
  - Runner thread integration with render->diff->write pipeline
affects: [future-direct-cell-writes, profiling, pgo]

# Tech tracking
tech-stack:
  added: []
  patterns: [double-buffered-cell-buffer, escape-string-parser, differential-terminal-output]

key-files:
  created: [tests/test_screen_buffer.cpp]
  modified: [src/btop_draw.hpp, src/btop_draw.cpp, src/btop.cpp, tests/CMakeLists.txt]

key-decisions:
  - "Incremental strategy (a): escape-string parser rather than direct cell writes -- ships differential rendering now, future optimization can migrate hot-path draw functions"
  - "Overlay/clock bypass screen buffer as ephemeral partial updates -- next full frame re-syncs buffer state"
  - "full_emit skips default (empty) cells for efficiency -- terminal already has blank space from clearing"
  - "Screen buffer force_full set on resize, redraw, and force_redraw to ensure complete repaint"
  - "Standard and bright colors stored as 256-color index+1 for uniform color handling in emit"

patterns-established:
  - "Cell color encoding: 0=default, bit24=truecolor(RGB in low 24), else 256-color index+1"
  - "Attribute bitfield: bit0=bold, bit1=dim, bit2=italic, bit3=underline, bit4=blink, bit5=strikethrough"
  - "Double-buffer swap pattern: render to current, diff against previous, swap after write"

requirements-completed: [REND-01]

# Metrics
duration: 5min
completed: 2026-02-27
---

# Phase 5 Plan 3: Differential Terminal Output Summary

**Double-buffered cell buffer with escape-string parser producing differential terminal output -- only changed cells emit escape sequences during steady-state rendering**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-27T19:23:04Z
- **Completed:** 2026-02-27T19:28:31Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Cell struct with UTF-8 character storage (1-4 bytes), packed truecolor/256-color fg/bg, and 6-bit attribute bitfield
- ScreenBuffer class with double-buffering (swap/resize/clear), force_full flag for resize/redraw
- Complete CSI escape-string parser handling cursor positioning (CUP, relative moves), SGR color/attributes (truecolor, 256-color, standard, bright, reset), and UTF-8 characters
- diff_and_emit produces minimal escape output by skipping unchanged cells, tracking last-emitted color/attr state, and omitting cursor moves for adjacent changed cells
- full_emit for force_redraw/resize with same color/attr optimization (skips redundant escape codes)
- Runner thread integrated: draw functions -> render_to_buffer -> diff_and_emit -> write_stdout
- 36 unit tests covering cell equality, buffer operations, escape parsing, diff correctness, and round-trip verification

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement Cell struct, ScreenBuffer, render_to_buffer, diff_and_emit** - `b391b3f` (feat)
2. **Task 2: Integrate ScreenBuffer into Runner thread** - `047e6cf` (feat)

## Files Created/Modified
- `src/btop_draw.hpp` - Added Cell struct, ScreenBuffer class, render_to_buffer/diff_and_emit/full_emit declarations
- `src/btop_draw.cpp` - Implemented ScreenBuffer, escape-string parser, differential/full emit algorithms with color/attr helpers
- `src/btop.cpp` - Added ScreenBuffer to Runner namespace, replaced direct string output with render->diff->write pipeline, added resize/init hooks
- `tests/test_screen_buffer.cpp` - 36 unit tests for cell equality, buffer ops, escape parsing, diff correctness, round-trip
- `tests/CMakeLists.txt` - Added test_screen_buffer.cpp to build

## Decisions Made
- Incremental strategy (a): escape-string parser rather than direct cell writes -- ships differential rendering now while preserving all existing draw functions unchanged
- Overlay/clock bypass screen buffer as ephemeral partial updates -- next full frame re-syncs buffer state correctly
- full_emit skips default (empty) cells -- reduces output size since terminal already has blank space
- Standard and bright terminal colors stored uniformly as 256-color index+1 alongside truecolor encoding

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed bright color test expectation**
- **Found during:** Task 1 (unit test verification)
- **Issue:** Test expected bright red (ESC[91m]) to store as index 9, but correct encoding is 91-90+8=9 stored as index+1=10
- **Fix:** Corrected test expectation from 9u to 10u
- **Files modified:** tests/test_screen_buffer.cpp
- **Verification:** All 36 tests pass
- **Committed in:** b391b3f (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Minor test expectation correction. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 5 (Rendering Pipeline) is now complete with all 3 plans executed
- Differential rendering is active: steady-state frames emit only changed cells
- Future optimization: migrate hot-path draw functions (Graph) to direct cell writes to eliminate parser overhead if profiling indicates it as a bottleneck
- Ready for Phase 6 (PGO) which will profile the optimized code

---
*Phase: 05-rendering-pipeline*
*Completed: 2026-02-27*
