---
gsd_state_version: 1.0
milestone: v1.3
milestone_name: Menu PDA + Input FSM
current_phase: 24
current_plan: Not started
status: completed
last_updated: "2026-03-02T14:08:02.184Z"
last_activity: 2026-03-02
progress:
  total_phases: 5
  completed_phases: 5
  total_plans: 9
  completed_plans: 9
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-02)

**Core value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.
**Current focus:** Phase 24 — Tests

## Current Position

**Current Phase:** 24
**Current Plan:** Not started
**Total Plans in Phase:** 2
**Status:** Milestone complete
**Last Activity:** 2026-03-02

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**
- Total plans completed: 1
- Average duration: 3min
- Total execution time: 0.05 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 20-pda-types-skeleton | 1 | 3min | 3min |

**Recent Trend:**
- Last 5 plans: 3min
- Trend: Starting

*Updated after each plan completion*
| Phase 21 P01 | 4min | 2 tasks | 4 files |
| Phase 21 P02 | 6min | 2 tasks | 1 files |
| Phase 22 P01 | 6min | 2 tasks | 3 files |
| Phase 22 P02 | 4min | 2 tasks | 2 files |
| Phase 23 P01 | 3min | 2 tasks | 3 files |
| Phase 23 P02 | 4min | 2 tasks | 5 files |
| Phase 24 P01 | 4min | 2 tasks | 3 files |
| Phase 24 P02 | 2min | 2 tasks | 0 files |

## Accumulated Context

### Decisions

Full decision log in PROJECT.md Key Decisions table.

Recent decisions affecting current work:
- Roadmap: 6-phase dependency chain (types -> statics -> dispatch -> input FSM -> tests -> cleanup) driven by hard code dependencies
- Research: PDAAction return-value pattern must be established in Phase 20 to prevent variant self-modification UB (pitfalls 1 and 7)
- Research: Layout/interaction struct split must be established in Phase 20 to prevent stale coordinates on resize (pitfall 6)
- Research: Single-writer wrapper (menu_show/menu_close) introduced in Phase 21 as migration safety net
- 20-01: Placed all PDA types in menu namespace following btop_state.hpp pattern
- 20-01: Used std::stack with vector backing for contiguous memory
- 20-01: Assert-based preconditions on pop/replace/top rather than exceptions
- [Phase 21]: Coexistence of bg and pda.bg() during migration -- both cleared in parallel until Plan 02 migrates functions
- [Phase 21]: PDA seeded lazily on first dispatch in process() via switch on currentMenu
- [Phase 21]: menu_close_current omits invariant assert since process() may close and reopen in sequence
- [Phase 21]: Moved Theme::updateThemes() into optionsMenu redraw block guarded by pda.bg().empty() instead of standalone sentinel
- [Phase 21]: Removed file-scope string bg entirely -- PDA owns bg lifecycle exclusively after all functions migrated
- [Phase 22]: dispatch_legacy bridge translates PDAResult to int codes, keeping process() unchanged for Plan 02
- [Phase 22]: Replace actions applied inside dispatch_legacy with menuMask/currentMenu sync for correct re-dispatch
- [Phase 22]: Handler rendered flag (bool did_render) replaces legacy int retval pattern (Changed/NoChange)
- [Phase 22]: Non-recursive dispatch loop replaces recursive process() -- while(true) with std::visit and PDAAction switch
- [Phase 22]: Mouse mappings from active frame copied to global Menu::mouse_mappings only on NoChange action
- [Phase 22]: menuMask, currentMenu, menuFunc, menuReturnCodes, dispatch_legacy fully deleted -- PDA is sole dispatch authority
- [Phase 23]: FSM transitions owned by Input code -- enter_menu() before Menu::show(), exit_menu() after PDA empties
- [Phase 23]: Menu::active temporarily read in process() to detect PDA empty -- bridge until Plan 02 removes it
- [Phase 23]: Filtering enter/exit paths replaced with transition functions; old_filter global deleted
- [Phase 23]: Runner::pause_output.load() replaces Menu::active as atomic cross-thread menu-activity signal
- [Phase 23]: Menu::invalidate_layout() wrapper keeps PDA file-scoped while exposing layout reset for resize
- [Phase 24]: Pure unit tests through public API only -- no extern of file-scoped fsm_state
- [Phase 24]: Test fixtures with SetUp/TearDown to prevent global state pollution between tests
- [Phase 24]: Config::set/getS works without Config::load for test fixtures -- static arrays have compile-time defaults
- [Phase 24]: Force FetchContent source build for GTest under sanitizer builds to avoid conda system GTest dylib mismatch
- [Phase 24]: No sanitizer suppressions needed -- all 330 tests pass clean under both ASan+UBSan and TSan

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-02
Stopped at: Completed 24-02-PLAN.md (Sanitizer-Clean Builds)
Resume file: None
