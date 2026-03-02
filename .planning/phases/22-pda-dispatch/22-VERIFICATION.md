---
phase: 22-pda-dispatch
verified: 2026-03-02T14:10:00Z
status: passed
score: 4/4 success criteria verified
gaps: []
human_verification:
  - test: "Run btop interactively — open Main menu, navigate to Options, press ESC, confirm return to normal (not Main)"
    expected: "ESC from Options brings user to the normal btop view, not back to the Main menu"
    why_human: "ESC-from-replace semantics are correct in code (Pop returns to empty stack -> Normal), but the end-to-end UX path requires a running binary and user interaction to confirm no regression"
  - test: "Open SignalChoose, attempt to send a signal to PID 1 (should fail), confirm SignalReturn screen appears"
    expected: "Error dialog shows after failed kill(); subsequent ESC closes the menu entirely"
    why_human: "Replace(SignalReturnFrame) + Pop chain behavior requires live process kill() to exercise"
---

# Phase 22: PDA Dispatch Verification Report

**Phase Goal:** The PDA is the sole dispatch authority for all menu rendering — menuMask, currentMenu, and menuFunc are fully replaced by std::visit on PDA top frame
**Verified:** 2026-03-02T14:10:00Z
**Status:** PASSED
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Menu::process() dispatches to the active menu via std::visit on PDA top frame; menuMask bitset, currentMenu int, and menuFunc vector are deleted | VERIFIED | `std::visit(MenuVisitor{current_key}, pda.top())` at line 1859; zero grep hits for menuMask/currentMenu/menuFunc across src/ |
| 2 | Main-to-Options and Main-to-Help transitions use replace() (ESC returns to Normal, not Main) | VERIFIED | mainMenu lines 1226/1228: `return {PDAAction::Replace, menu::OptionsFrame{}}` / `return {PDAAction::Replace, menu::HelpFrame{}}`. optionsMenu ESC line 1402: `return {PDAAction::Pop}` (pops Replace frame back to empty stack → Normal) |
| 3 | bg string lifecycle is tied to stack depth: captured on first push, cleared on final pop | VERIFIED | pda.set_bg() in handler redraw blocks (lines 997, 1199, 1335, 1676, 1728); pda.clear_bg() in process() on empty-stack (line 1832), Pop (line 1881), and Replace (line 1892) |
| 4 | The Switch return-code re-entrant call pattern is eliminated — all frame transitions return PDAAction values applied by the caller after the visitor completes | VERIFIED | process() is a non-recursive while-loop (lines 1821–1909) with no self-call inside the body; "process()" appears only in comment (line 1816), header comment (line 1912), and show() call (line 1928). No menuReturnCodes enum, no dispatch_legacy, no recursive process() call |

**Score:** 4/4 success criteria verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/btop_menu_pda.hpp` | PDAResult struct with action/next_frame/rendered fields | VERIFIED | Lines 186–190: `struct PDAResult { PDAAction action{PDAAction::NoChange}; std::optional<MenuFrameVar> next_frame{}; bool rendered{false}; };`. `#include <optional>` present at line 26. |
| `src/btop_menu.cpp` | Non-recursive process() loop with std::visit dispatch; 8 handlers with PDAResult signatures; MenuVisitor struct | VERIFIED | MenuVisitor at lines 1802–1813; 8 handlers at lines 982, 1091, 1113, 1155, 1187, 1264, 1659, 1712; process() while-loop at lines 1821–1909 |
| `src/btop_menu.hpp` | Clean header — menuMask and output extern removed | VERIFIED | No `bitset`, no `menuMask`, no `extern string output`. Confirmed: `grep -c "menuMask\|Menu::output" src/btop_menu.hpp` = 0 |
| `tests/test_menu_pda.cpp` | 5 PDAResult unit tests | VERIFIED | Lines 103–133: DefaultConstruction, PopHasNoPayload, ReplaceCarriesFrame, PushCarriesFrame, RenderedFlag. All 5 pass. |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `btop_menu.cpp` (process loop) | `btop_menu_pda.hpp` (PDAAction) | `switch (result.action)` with `case PDAAction::{Pop,Replace,Push,NoChange}` | VERIFIED | Lines 1862–1907: all four PDAAction arms handled |
| `btop_menu.cpp` (process) | `Menu::mouse_mappings` | `std::visit([](const auto& f) { mouse_mappings = f.mouse_mappings; }, pda.top())` in NoChange case | VERIFIED | Lines 1864–1867: frame mouse_mappings copied to global on NoChange only; cleared on Pop/Replace |
| `btop_menu.cpp` (mainMenu) | `PDAAction::Replace` with OptionsFrame/HelpFrame payload | Return values at lines 1226/1228 | VERIFIED | `return {PDAAction::Replace, menu::OptionsFrame{}}` and `return {PDAAction::Replace, menu::HelpFrame{}}` |
| `btop_menu.cpp` (MenuVisitor) | `btop_menu_pda.hpp` (PDAResult) | `PDAResult operator()` per frame type | VERIFIED | All 8 overloads confirmed at lines 1805–1812 |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| PDA-04 | 22-01-PLAN.md, 22-02-PLAN.md | PDA dispatch replaces menuMask bitset + currentMenu int + menuFunc dispatch vector | SATISFIED | `grep -c "menuMask\|currentMenu\|menuFunc" src/btop_menu.cpp` = 0; `std::visit(MenuVisitor{current_key}, pda.top())` is the sole dispatch path |
| PDA-06 | 22-01-PLAN.md, 22-02-PLAN.md | replace() operation handles Main→Options and Main→Help transitions (ESC returns to Normal, not Main) | SATISFIED | mainMenu returns `PDAAction::Replace` (not Push) for Options/Help; ESC in optionsMenu/helpMenu returns `PDAAction::Pop` which drains the stack back to empty → Normal state |

No orphaned requirements found. REQUIREMENTS.md Traceability table confirms both PDA-04 and PDA-06 map to Phase 22 with status Complete.

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/btop_menu.cpp` | 1742 | `// TODO: show error message` in reniceMenu (Proc::set_priority failure path) | Info | Pre-existing from Phase 21 (confirmed via `git show 20d2bda`). Not introduced by Phase 22. Does not affect dispatch correctness. |

No blockers or warnings introduced by Phase 22. The single TODO is inherited from the prior phase and does not prevent the phase goal from being achieved.

---

### Human Verification Required

#### 1. ESC-from-Options returns to Normal (not Main)

**Test:** Open btop, press 'm' to open Main menu, navigate to Options, press ESC.
**Expected:** The normal btop dashboard appears (not the Main menu). No second ESC should be needed.
**Why human:** The Replace-then-Pop chain is structurally correct: Main push replaces to Options, ESC Pops Options leaving empty stack which makes process() set `Menu::active = false` and return to the dashboard. But the full visual UX path requires a running binary with a live terminal.

#### 2. SignalSend failure path shows SignalReturn screen

**Test:** Open btop, select a process, press 'k', confirm the signal send to a privileged PID (e.g. PID 1). The kill() call should fail.
**Expected:** After failed kill(), the SignalReturn screen appears. ESC from SignalReturn closes the menu entirely (not back to SignalChoose or Main).
**Why human:** signalSend returns `{PDAAction::Replace, menu::SignalReturnFrame{}}` on kill error, which is structurally verified; the end-to-end path requires actually triggering a kill() failure at runtime.

---

### Gaps Summary

No gaps. All four success criteria are fully achieved in the codebase:

1. **Sole dispatch authority** — `std::visit(MenuVisitor{current_key}, pda.top())` is the one and only dispatch path in process(). The legacy menuMask bitset, currentMenu int, and menuFunc vector have zero references in all source files.

2. **Replace for Main→Options/Help** — mainMenu explicitly returns `{PDAAction::Replace, menu::OptionsFrame{}}` and `{PDAAction::Replace, menu::HelpFrame{}}`. Because Replace is used (not Push), ESC from Options/Help pops the single-depth stack to empty, triggering the "no menu active" path in process() — returning to Normal, not Main.

3. **bg lifecycle** — pda.set_bg() is called in each handler's redraw block (signalChoose, mainMenu, optionsMenu, helpMenu, reniceMenu). pda.clear_bg() is called in process() on empty-stack, Pop, and Replace paths. sizeError correctly has no set_bg (it is the fallback screen with no bg overlay).

4. **Non-recursive loop** — process() is a `while(true)` loop with zero self-calls. The old Switch re-entrant pattern (where a handler called pda.replace() and returned Switch, causing process() to recurse) is fully eliminated. All frame transitions now return PDAResult values that process() applies after the visitor returns.

All 312 unit tests pass. Four task commits verified: cad1eb2 (PDAResult struct + tests), 1f9e1d4 (8 handler signatures + MenuVisitor), 9d00b6b (non-recursive process()), a750bda (header cleanup).

---

_Verified: 2026-03-02T14:10:00Z_
_Verifier: Claude (gsd-verifier)_
