---
phase: 21-static-local-migration
verified: 2026-03-02T12:00:00Z
status: passed
score: 10/10 must-haves verified
re_verification: false
---

# Phase 21: Static Local Migration Verification Report

**Phase Goal:** All 8 menu rendering functions consume per-frame state from frame structs instead of function-static locals, while menuMask/currentMenu remain the dispatch authority during migration
**Verified:** 2026-03-02
**Status:** passed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

Truths are derived from ROADMAP.md Success Criteria and plan frontmatter `must_haves`. Both plans are verified together as they combine to deliver the phase goal.

#### Plan 01 Truths (FRAME-03 — invalidate_layout + PDA infrastructure)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Every frame struct has an `invalidate_layout()` that zeros layout fields and clears mouse_mappings | VERIFIED | 8 frame structs + MenuPDA class all have the method (9 implementations at lines 56, 82, 98, 105, 119, 126, 133, 148, 247 of `btop_menu_pda.hpp`) |
| 2 | `invalidate_layout()` preserves interaction fields (selected, page, editing, etc.) | VERIFIED | 10 `InvalidateLayout` tests pass; each verifies layout zeroed AND interaction fields preserved |
| 3 | A file-scope `menu::MenuPDA pda` instance exists in `btop_menu.cpp` | VERIFIED | Line 65 of `btop_menu.cpp`: `menu::MenuPDA pda;` inside `namespace Menu {}` |
| 4 | Single-writer wrappers (`menu_open`, `menu_close_current`, `menu_clear_all`) keep menuMask and PDA stack in sync | VERIFIED | Lines 1829–1844 define all three wrappers; `show()` routes all 8 menu cases through `menu_open()` (lines 1916–1923); SizeError override uses `menu_clear_all()` + `menu_open()` (lines 1868–1869) |
| 5 | `process()` and `show()` use the wrappers instead of direct menuMask mutation | VERIFIED | All menuMask mutations in `process()` and `show()` go through the three wrappers; no bare `menuMask.set/reset()` in process/show bodies |
| 6 | `assert(menuMask.none() == pda.empty())` invariant guard is active | VERIFIED | Line 1832 (in `menu_open`) and line 1844 (in `menu_clear_all`) both have the assert |

#### Plan 02 Truths (FRAME-01 — menu function migration)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 7 | Every menu function reads/writes from its frame struct instead of function-static locals | VERIFIED | 5 functions have active `auto& frame = std::get<XxxFrame>(pda.top())` + auto& aliases (signalChoose l.990, mainMenu l.1196, optionsMenu l.1280, helpMenu l.1676, reniceMenu l.1730); 3 functions (sizeError, signalSend, signalReturn) had zero mutable static locals to migrate — frame line commented as "available if needed" per Plan 02 Task 1 |
| 8 | No mutable static locals remain in any of the 8 menu function bodies | VERIFIED | `grep -n "^\t\tstatic " btop_menu.cpp \| grep -v "static const"` returns zero hits; remaining `static int funcName()` are function declarations, not locals |
| 9 | The `bg.empty()` sentinel pattern is eliminated from all menu functions | VERIFIED | `grep -n "bg\.empty()"` returns zero hits in `btop_menu.cpp` |
| 10 | All bg reads/writes use `pda.bg()/pda.set_bg()/pda.clear_bg()` | VERIFIED | 14 references to `pda.set_bg`, `pda.bg()`, `pda.clear_bg` found; file-scope `string bg` declaration removed entirely (zero hits for `^string bg` or `^\tstring bg`) |

**Score:** 10/10 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/btop_menu_pda.hpp` | `invalidate_layout()` on all 8 frame structs + `MenuPDA::invalidate_layout()` | VERIFIED | 255 lines; 9 `void invalidate_layout()` implementations; all bodies are substantive (zero layout fields, clear mouse_mappings); no stubs |
| `src/btop_menu.cpp` | File-scope PDA instance, single-writer wrappers, adapted process()/show(), all 8 functions migrated | VERIFIED | 1928 lines; `menu::MenuPDA pda` at line 65; 3 wrappers at lines 1829–1844; `std::get<menu::*>` in 5 of 8 functions (3 have no static locals to migrate); `string bg` file-scope removed |
| `tests/test_menu_pda.cpp` | `InvalidateLayout` test group with tests for all frame types | VERIFIED | 10 tests in `InvalidateLayout` group covering all 8 frame types, `MenuPDA::invalidate_layout()` dispatch, and empty-PDA no-op safety |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `btop_menu.cpp (menu_open)` | `btop_menu_pda.hpp (MenuPDA::push)` | `pda.push()` paired with `menuMask.set()` | WIRED | Line 1829–1832: `menuMask.set(menu_enum); pda.push(std::move(frame));` with assert |
| `btop_menu.cpp (process)` | `btop_menu.cpp (menu_close_current)` | Closed return code triggers `pda.pop()` | WIRED | Line 1893: `menu_close_current(currentMenu)` in Closed handler |
| `btop_menu.cpp (mainMenu)` | `btop_menu_pda.hpp (MainFrame)` | `std::get<menu::MainFrame>(pda.top())` | WIRED | Line 1196: active frame retrieval with auto& aliases for y, selected, colors_selected, colors_normal |
| `btop_menu.cpp (optionsMenu)` | `btop_menu_pda.hpp (OptionsFrame)` | `std::get<menu::OptionsFrame>(pda.top())` | WIRED | Line 1280: active frame retrieval with 14 auto& aliases covering all mutable state |
| `btop_menu.cpp (mainMenu Switch)` | `btop_menu.cpp (pda.replace)` | `pda.replace(menu::OptionsFrame{})` on Main→Options transition | WIRED | Lines 1235–1242: `pda.replace(menu::OptionsFrame{})` and `pda.replace(menu::HelpFrame{})` paired with `menuMask.set()` and `currentMenu =` update |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| FRAME-01 | 21-02-PLAN.md | Each frame struct carries per-frame state as members (replacing function-static locals) | SATISFIED | All 5 functions with per-frame state use `std::get<XxxFrame>(pda.top())` + auto& aliases; 3 functions with no state have frame line commented; zero mutable static locals remain |
| FRAME-03 | 21-01-PLAN.md | `invalidate_layout()` zeros layout fields while preserving interaction fields | SATISFIED | 8 frame structs + MenuPDA::invalidate_layout() exist in `btop_menu_pda.hpp`; 10 tests pass verifying correct behavior |

**Orphaned requirements check:** REQUIREMENTS.md maps FRAME-01 and FRAME-03 to Phase 21 only. No additional Phase 21 requirements exist in REQUIREMENTS.md that are unaccounted for.

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/btop_menu.cpp` | 1021, 1025 | Direct `menuMask.set(SignalReturn)` in `signalChoose` body (not through `menu_open` wrapper) | Info | Intentional by design — documented in Plan 02 key-decisions: "Kept menuMask.set(SignalReturn) in signalChoose/signalSend as-is -- PDA catch-up handled by process() lazy seeding" |
| `src/btop_menu.cpp` | 1143 | Direct `menuMask.set(SignalReturn)` in `signalSend` body | Info | Same as above — same documented decision |
| `src/btop_menu.cpp` | 1235, 1240 | Direct `menuMask.set(Menus::Options/Help)` in `mainMenu` Switch paths | Info | Intentional — Plan 02 Task 1 explicitly specifies this pattern for Switch paths, paired with `pda.replace()`. Not a regression. |
| `src/btop_menu.cpp` | 1099, 1122, 1163 | Frame retrieval lines commented out in sizeError, signalSend, signalReturn | Info | Intentional — these functions have no per-frame state to access; Plan 02 Task 1 explicitly notes "add frame retrieval line but it may go unused" |
| `src/btop_menu.cpp` | 1759 | `// TODO: show error message` in reniceMenu | Warning | Pre-existing TODO, not introduced in Phase 21; does not affect migration goal |

No blockers found. All anti-patterns are either intentional per-plan decisions or pre-existing.

---

### Human Verification Required

**1. Menu Runtime Behavior**
**Test:** Launch btop, open each of the 8 menus (F1=Main, Options from main, Help from main, kill a process via SignalChoose/SignalSend/SignalReturn, resize terminal to force SizeError, Renice a process), navigate within each, and close via Escape.
**Expected:** All menus open, display correctly, navigate, and close with no visual artifacts. On terminal resize while a menu is open, layout resets cleanly while selection state (e.g., current page, selected item) is preserved.
**Why human:** Actual rendering output, real OS signals (kill/renice), and terminal resize behavior cannot be verified programmatically in this environment.

**2. invalidate_layout on Resize**
**Test:** Open a menu (e.g., Options), resize the terminal, observe the menu redraws with correct geometry.
**Expected:** Options menu recalculates x/y/height to fit new terminal dimensions; selected item and page number are preserved from before the resize.
**Why human:** Requires visual inspection of live terminal resize behavior; cannot be triggered via unit tests alone.

---

### Build and Test Verification

| Check | Command | Result |
|-------|---------|--------|
| btop binary compiles | `cmake --build build-test --target btop` | PASS — `[100%] Built target btop` |
| btop_test binary compiles | `cmake --build build-test --target btop_test` | PASS — `[100%] Built target btop_test` |
| InvalidateLayout tests | `ctest -R InvalidateLayout` | PASS — 10/10 tests passed |
| Full test suite | `ctest --test-dir build-test` | PASS — 307/307 tests passed, 0 failures |

---

### Gaps Summary

No gaps. All 10 truths verified. Both FRAME-01 and FRAME-03 requirements are satisfied. The build is clean and the full test suite passes with 307 tests. The three direct `menuMask.set()` calls inside menu function bodies (signalChoose, signalSend, mainMenu Switch paths) are intentional per documented plan decisions and do not represent missing implementation.

---

_Verified: 2026-03-02_
_Verifier: Claude (gsd-verifier)_
