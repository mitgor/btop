# Feature Research: Menu PDA + Input FSM

**Domain:** Menu pushdown automaton and input finite state machine for btop++ C++ terminal monitor
**Researched:** 2026-03-02
**Confidence:** HIGH (codebase analysis + established TUI/game-state-machine literature)

---

## Context: What Exists and What Changes

### Existing (keep, do not break)

| Component | Current Encoding | Problem |
|-----------|-----------------|---------|
| 8 menus (Main, Options, Help, SizeError, SignalChoose, SignalSend, SignalReturn, Renice) | `bitset<8> menuMask` + `int currentMenu` | Multiple bits can be set simultaneously; `currentMenu` is the last set bit — implicit priority |
| Menu per-frame state | `static` locals inside each `menuFuncN()` function | Survives across calls, not reset on re-entry, hidden lifetime |
| Menu transitions | `menuReturnCodes`: NoChange / Changed / Closed / Switch | `Switch` replaces current; `Closed` pops via `menuMask.reset(currentMenu)` |
| Input routing split | `Menu::active` bool checked in `Input::get()` mouse routing; `Input::process()` checks `Config::proc_filtering` bool | Two independent conditionals, not a typed FSM |
| Filtering state | `Config::proc_filtering` bool + `old_filter` string | Global config bool used as mode flag |

### Target (v1.3 goal)

- `std::vector<MenuFrame>` stack where `MenuFrame = std::variant<frame::Main, frame::Options, frame::Help, ...>` — each frame carries its own per-frame data
- `push(frame)` → enter menu; `pop()` → return to previous (or close all if stack empties)
- Input FSM: `std::variant<input::Normal, input::Filtering, input::MenuActive>` replacing the `Menu::active` bool + `proc_filtering` bool pair
- Zero user-visible behavior change

---

## Feature Landscape

### Table Stakes (Must Have for This Milestone)

Features that the PDA and Input FSM must provide. Missing any one = the migration is incomplete, regression exists, or the architecture goal is unmet.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Typed stack frames for all 8 menus | The entire point of the milestone; replaces `bitset<8>` + `currentMenu` | MEDIUM | Each frame variant alternative carries its own state (e.g., `frame::SignalChoose { int selected_signal; int x; int y; }`) replacing function-static locals |
| `push(frame)` operation | Entering a menu from Normal or from another menu must put the new frame on top | LOW | Replace `menuMask.set(menu)` call sites; preserve caller frame below |
| `pop()` operation | Closing a menu (ESC/q/Closed return code) must restore the frame beneath it | LOW | Replace `menuMask.reset(currentMenu)` + recursive `process()` call; if stack empties, transition input FSM back to Normal |
| Clean per-frame state initialization | Frame state initializes when pushed, not from a prior static | MEDIUM | Current code uses `if (bg.empty()) selected = 0;` hacks to detect re-entry; frame constructor sets initial state cleanly |
| Clean per-frame state destruction on pop | Frame state destroyed when popped — no dangling statics | LOW | RAII via variant destruction; function-static locals eliminated |
| `peek()` / top-frame dispatch | `process(key)` must dispatch input to the top frame only | LOW | Replaces `menuFunc.at(currentMenu)(key)` dispatch |
| Background capture on push (not per-frame) | `bg` string (the blurred background) is captured once when first menu opens, not re-captured per frame | LOW | Current: `bg` is a single module-level string; behavior unchanged — bg cleared only when stack fully empties |
| `mouse_mappings` reset on frame transition | Mouse mappings belong to the active frame; must be cleared and rebuilt when pushing or popping | LOW | Current: `mouse_mappings.clear()` on Closed/Switch; must happen on push and pop |
| `Switch` return code → replace frame, not push | Main→Options/Help does a replace (not a push-over-main), so going back to main after closing Options does NOT re-show Main | MEDIUM | Distinguish `push()` (depth increases) from `replace()` (depth unchanged) or model Main→Options as `pop(); push(Options)` — either way must match current behavior where ESC from Options goes to Normal, not Main |
| Input FSM: `Normal` state | Keyboard goes to `Input::process()` for global + box-specific handlers | LOW | Replaces implicit "not filtering, not in menu" condition |
| Input FSM: `Filtering` state | Keyboard routed to `Proc::filter.command(key)` + filter commit/cancel | LOW | Replaces `Config::proc_filtering` bool; carries `old_filter` string in state |
| Input FSM: `MenuActive` state | Keyboard routed to `Menu::process(key)` only | LOW | Replaces `Menu::active` atomic bool; eliminates the bool from `Input::get()` mouse routing check |
| Input FSM transition Normal → Filtering | `f` or `/` key from Normal state | LOW | Entry action: set `Proc::filter = Draw::TextEdit{...}`, save `old_filter` |
| Input FSM transition Filtering → Normal | `enter`, `escape`, `mouse_click` from Filtering state | LOW | Entry/exit actions: commit or restore filter value |
| Input FSM transition Normal ↔ MenuActive | `Menu::show()` / `Menu::process()` with empty stack triggers Normal | LOW | When PDA stack goes empty, input FSM pops to Normal |
| Mouse routing uses Input FSM state | `Input::get()` checks input FSM state (not `Menu::active` bool) for which mouse_mappings to use | LOW | Single source of truth replaces `Menu::active` + config bool |
| Comprehensive tests for PDA transitions | Push, pop, replace, empty-stack, size-error override all tested | HIGH | Following v1.1 pattern: 266+ passing tests, ASan+UBSan+TSan clean |
| Comprehensive tests for Input FSM transitions | Normal→Filtering→Normal, Normal→MenuActive→Normal, key routing verified per state | MEDIUM | Unit-testable because state is a typed variant, not global bools |
| Zero regression in user-visible behavior | Same menus, same keys, same flow, same visuals | HIGH (validation) | Verified by existing test suite + ASan/UBSan/TSan clean builds |

### Differentiators (Architectural Improvements Enabled by PDA)

Features that the PDA/FSM architecture makes possible or significantly cleaner — not strictly required for behavior parity, but are the architectural payoff of the migration.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Illegal menu combinations unrepresentable | `bitset<8>` allows `menuMask = 0b11111111` (all 8 menus active). Variant stack allows only one frame per stack slot — simultaneous conflicting menus are structurally impossible | LOW (side-effect of design) | No explicit code to write; falls out of the typed variant stack |
| Per-frame state lifetime matches frame lifetime | `frame::SignalChoose` carries `selected_signal` as a member; it's initialized on push and destroyed on pop — no `static` lifetime pollution across close/reopen cycles | LOW (side-effect of design) | Eliminates the `if (bg.empty()) selected = 0;` re-entry detection hacks in all 8 menu functions |
| Compiler-enforced frame handling | `std::visit` on `MenuFrame` variant produces compile error if a new frame type is added without handling it in dispatch | LOW (side-effect of design) | Makes future menu additions safe |
| Transition logging | Every push/pop/replace can log `"[Menu PDA] push(Options) → stack: [Main, Options]"` for debuggability | LOW | One log statement in push/pop/replace |
| Input mode visible at a glance | `std::holds_alternative<input::Filtering>(input_state)` is self-documenting; `Config::getB(BoolKey::proc_filtering)` is not | LOW (side-effect of design) | Debugging and future development benefit |
| Old filter preserved in state struct | `input::Filtering { string old_filter; Draw::TextEdit filter; }` keeps filter rollback data in the FSM state, not in a module-level `string old_filter` global | LOW | Eliminates one global variable |
| Extensible focus model | Once input routing is typed, adding a third input mode (e.g., a vim-style command mode) is additive — add a new alternative to the InputFSM variant and handlers | LOW (future-facing) | Not for v1.3, but architecture now supports it |
| Stack depth limit (defensive) | Assert/enforce `stack.size() <= 3` to prevent any pathological push loop that the bitset could not express but the stack could | LOW | Defensive guard; `bitset<8>` was naturally bounded to 8 |

### Anti-Features (Do Not Build)

Features that seem natural to add during this refactor but would violate scope, add risk, or create problems.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Deep push of Main on top of Options/Help | PDA pattern suggests "push every menu on stack" — so opening Options from Main would push Options over Main | Current btop behavior: ESC from Options goes to Normal, NOT back to Main. Users do not expect to return to Main by closing Options. Breaking this is a regression | Use `replace()` for Main→Options/Help transitions; reserve `push()` for cases that genuinely need return (e.g., SignalChoose → SignalReturn) |
| Animate or transition visuals during push/pop | Stack-based UIs often have slide or fade transitions | Zero visual change is the explicit constraint in PROJECT.md; any animation adds terminal I/O complexity and is out of scope | Keep the existing instant-switch rendering behavior |
| History states (remember substate when re-entering) | PDA stacks often carry a "return to where we were" concept — e.g., remember which option was selected when reopening Options | btop already does this via function-static locals (e.g., `static int selected_cat` persists). The migration goal is to move this state into the frame struct, not to change the behavior | Move static locals into frame struct members; preserve current persistence semantics |
| Async menu state machines | Run menu update logic on a separate thread or with coroutines | Menu rendering must coordinate with Runner's pause_output and bg string; async would introduce races. The existing synchronous model is correct | Keep menu processing synchronous on the main thread |
| Boost.SML or external FSM library | External libraries provide DSL, hierarchy, guards | PROJECT.md: no new dependencies. Boost.SML would be a new dependency. `std::variant` + `std::visit` is established in this codebase (v1.1 pattern) | Use `std::variant` + `std::visit` matching the existing App/Runner FSM pattern |
| Replacing `Menu::msgBox` class | The msgBox helper could be refactored to also be a frame type | It works, it's tested, it's not the migration target. Touching it risks regression in SizeError, SignalSend, SignalReturn, ReniceMenu behaviors | Leave msgBox as-is; just move its state into the frame struct (e.g., `frame::SizeError { msgBox box; }`) |
| Input FSM carrying full keyboard event history | An `InputFSM` could carry a ring buffer of recent keys for multi-key sequence detection | btop already has `Input::history` deque. Adding another history buffer in the FSM state struct duplicates data | Leave `Input::history` deque in place; `InputFSM` state struct carries only mode-specific data |
| Global input FSM accessible across threads | Making the input FSM readable from Runner thread | Input processing is main-thread only. The runner thread does not need to know the input mode. Cross-thread access would require atomics, undoing the simplification | Keep input FSM as main-thread-only, like the App/Runner FSM variant |

---

## Feature Dependencies

```
[PDA: Typed Stack Frames]
    +-- requires --> [Frame variant definition]   (MenuFrame = std::variant<...>)
    +-- requires --> [Per-frame state structs]     (frame::Options { int selected; int selected_cat; ... })
    +-- enables  --> [Clean state initialization] (constructor replaces if (bg.empty()) hacks)
    +-- enables  --> [Illegal state prevention]   (structural; falls out of variant)

[PDA: push() operation]
    +-- requires --> [PDA: Typed Stack Frames]
    +-- requires --> [Input FSM: MenuActive state] (push must transition input FSM if stack was empty)
    +-- replaces --> [menuMask.set() call sites]

[PDA: pop() operation]
    +-- requires --> [PDA: Typed Stack Frames]
    +-- enables  --> [Input FSM: Normal state]    (pop when stack empties → input FSM back to Normal)
    +-- replaces --> [menuMask.reset() call sites]
    +-- coordinates --> [bg string lifecycle]     (bg clears only when stack fully empties)

[PDA: replace() operation]
    +-- requires --> [PDA: Typed Stack Frames]
    +-- requires --> [understanding of Switch return code semantics]  (Main→Options must use replace, not push)

[Input FSM: typed states]
    +-- requires --> [InputFSMState = std::variant<input::Normal, input::Filtering, input::MenuActive>]
    +-- replaces --> [Menu::active atomic<bool>]
    +-- replaces --> [Config::proc_filtering bool (as routing signal)]
    +-- enables  --> [Mouse routing via FSM]      (Input::get() checks variant, not bool)

[Input FSM: Filtering state]
    +-- carries  --> [old_filter string]          (moves from module-level global into state struct)
    +-- carries  --> [Draw::TextEdit filter]      (per-activation filter edit context)

[Tests: PDA transitions]
    +-- requires --> [PDA: push/pop/replace]
    +-- requires --> [frame state structs are constructible/inspectable in tests]

[Tests: Input FSM transitions]
    +-- requires --> [Input FSM: typed states]
    +-- independent of --> [Tests: PDA transitions] (can test separately)
```

### Dependency Notes

- **`replace()` semantics must be decided before coding push/pop:** The Main→Options transition currently uses `Switch` return code (same menu slot, replaces bg) so ESC from Options returns to Normal, not to Main. The PDA must model this as `pop(); push(Options)` or a true `replace()`. The decision affects how many frames are on the stack and whether there is ever a Main frame below another frame.
- **Input FSM must transition to MenuActive when first menu is pushed:** The trigger for `Menu::active = true` must move from `process()` to the `push()` operation. This is the coupling point between the two new machines.
- **Mouse routing couples Input FSM and PDA:** `Input::get()` currently checks `Menu::active` to choose `Menu::mouse_mappings` vs `Input::mouse_mappings`. After migration, it checks the Input FSM state. The PDA's `push()` and `pop()` must correctly drive the Input FSM state, otherwise mouse events mis-route.
- **`bg` string lifetime is tied to the full stack, not individual frames:** `bg` is regenerated when `redraw = true` and is shared by the entire menu overlay. It clears when the stack fully empties. Individual frame push/pop must not clear `bg` unless the stack becomes empty — this is a subtlety of the current architecture that must be preserved.

---

## MVP Definition

This milestone is a refactoring milestone, not a feature-addition milestone. The MVP is identical to the existing behavior, with internal representation changed.

### Core PDA (v1.3 launch)

The minimum that constitutes a successful migration:

- [ ] `MenuFrame` variant defined with all 8 frame types, each carrying its per-frame state as struct members
- [ ] `std::vector<MenuFrame> menu_stack` replaces `bitset<8> menuMask` + `int currentMenu`
- [ ] `push(MenuFrame)` implemented — sets `Menu::active`, drives Input FSM transition
- [ ] `pop()` implemented — resets state, drives Input FSM, clears `bg` if stack empties
- [ ] `replace(MenuFrame)` implemented for Switch-return-code transitions (Main→Options/Help)
- [ ] All 8 menu functions refactored to receive their frame by reference instead of using function-static locals
- [ ] All `menuMask.set()` / `menuMask.reset()` / `currentMenu = ` call sites replaced
- [ ] `Menu::show()` and `Menu::process()` updated to work with stack

### Core Input FSM (v1.3 launch)

- [ ] `InputFSMState` variant defined: `input::Normal`, `input::Filtering { old_filter, filter }`, `input::MenuActive`
- [ ] `Input::process()` dispatches based on FSM state instead of `filtering` bool check
- [ ] `Input::get()` mouse routing uses FSM state instead of `Menu::active` bool
- [ ] Transitions Normal→Filtering, Filtering→Normal, Normal→MenuActive, MenuActive→Normal all wired to correct triggers
- [ ] `Menu::active` atomic bool removed (or aliased from FSM state for backward compatibility during transition)
- [ ] `Config::proc_filtering` bool no longer used as an input routing signal (may remain as a config value, but Input FSM state is the routing authority)

### Tests (v1.3 launch)

- [ ] PDA push/pop/replace transition table tested (all 8 frame types, empty-stack pop, SizeError override, stack depth guard)
- [ ] Input FSM state transitions tested (each edge, key routing per state)
- [ ] ASan + UBSan + TSan clean
- [ ] Existing 266 tests still pass

### Deferred (v1.4+)

- [ ] Multi-level push (e.g., a future nested dialog within Options) — architecture supports it, not needed for behavior parity
- [ ] Transition logging infrastructure — useful for debugging, not required for correctness

---

## Feature Prioritization Matrix

| Feature | Architectural Value | Implementation Cost | Regression Risk | Priority |
|---------|---------------------|---------------------|-----------------|----------|
| `MenuFrame` variant + frame structs | HIGH (foundation) | MEDIUM | LOW | P1 |
| push() / pop() / replace() | HIGH (core ops) | MEDIUM | MEDIUM | P1 |
| Input FSM `Normal` / `MenuActive` states | HIGH (replaces active bool) | LOW | LOW | P1 |
| Input FSM `Filtering` state | HIGH (replaces proc_filtering routing) | LOW | MEDIUM | P1 |
| Mouse routing via Input FSM | MEDIUM | LOW | MEDIUM | P1 |
| Per-frame state in struct (not static locals) | HIGH (eliminates hacks) | HIGH | MEDIUM | P1 |
| PDA transition tests | HIGH (safety net) | HIGH | N/A | P1 |
| Input FSM transition tests | HIGH (safety net) | MEDIUM | N/A | P1 |
| Stack depth guard | LOW (defensive) | LOW | LOW | P2 |
| Transition logging | LOW (debug aid) | LOW | LOW | P2 |
| Remove `Menu::active` bool | MEDIUM | LOW | LOW | P2 (after P1 validated) |

**Priority key:**
- P1: Required for v1.3 — the migration is not done without these
- P2: Cleanup after P1 is validated; safe to add in later plan within same milestone

---

## Implementation Notes (from Codebase Analysis)

### Frame State Inventory

Each menu function currently uses function-statics. These must become frame struct members:

| Frame | Function-Static State | Notes |
|-------|-----------------------|-------|
| `frame::Main` | `int y`, `int selected`, `vector<string> colors_selected`, `vector<string> colors_normal` | `selected` resets when `bg.empty()` |
| `frame::Options` | `int x`, `int y`, `int height`, `int page`, `int pages`, `int selected`, `int select_max`, `int item_height`, `int selected_cat`, `int max_items`, `int last_sel`, `bool editing`, `Draw::TextEdit editor`, `string warnings`, `bitset<8> selPred` | Most complex frame — 15 state members |
| `frame::Help` | `int x`, `int y`, `int height`, `int page`, `int pages` | Simpler |
| `frame::SizeError` | `msgBox messageBox` | msgBox carries its own state |
| `frame::SignalChoose` | `int x`, `int y`, `int selected_signal` | `selected_signal` resets when `bg.empty()` |
| `frame::SignalSend` | `msgBox messageBox` | Signal to send comes from PDA context |
| `frame::SignalReturn` | `msgBox messageBox` | Error code comes from PDA context |
| `frame::Renice` | `int x`, `int y`, `int selected_nice`, `string nice_edit` | `selected_nice` / `nice_edit` reset when `bg.empty()` |

### Current Menu Transition Graph

Understanding which transitions use `Switch` (replace) vs `Closed` (pop) is critical for PDA design:

```
Normal
  +--[show(Main)]----------> push(Main)
  +--[show(Options)]--------> push(Options)    [direct open, not via Main]
  +--[show(Help)]-----------> push(Help)       [direct open, not via Main]
  +--[show(SizeError)]------> push(SizeError)  [replaces all others if too small]
  +--[show(SignalChoose)]---> push(SignalChoose)
  +--[show(SignalSend)]-----> push(SignalSend)  [direct, from k/K key]

Main
  +--[select Options, enter]--> replace(Options)  [Switch return code]
  +--[select Help, enter]-----> replace(Help)     [Switch return code]
  +--[escape/q/m]-------------> pop() → Normal

Options
  +--[escape]----------------> pop() → Normal

Help
  +--[escape]----------------> pop() → Normal

SizeError
  +--[enter/escape]-----------> pop() → Normal  [or to whatever was below]

SignalChoose
  +--[enter + kill succeeds]--> pop() → Normal  [Closed, no SignalReturn needed]
  +--[enter + kill fails]-----> set(SignalReturn) then pop() → Normal  [then show(SignalReturn) fires]
  +--[escape/q]--------------> pop() → Normal

SignalSend
  +--[yes + kill succeeds]---> pop() → Normal
  +--[yes + kill fails]------> set(SignalReturn) then pop() → Normal
  +--[no/escape]-------------> pop() → Normal

SignalReturn
  +--[enter/escape]-----------> pop() → Normal

Renice
  +--[enter/escape]-----------> pop() → Normal
```

Key insight: `Switch` return code currently means "replace current frame AND reset bg/redraw" — it is used only for the Main→Options and Main→Help transitions. All other transitions use `Closed` (pop). The `menuMask.set(SignalReturn)` inside SignalChoose/SignalSend is a "schedule next menu after popping this one" side-effect — the PDA must model this as a post-pop push, not a concurrent set.

### Input FSM Trigger Map

| Current code | FSM equivalent |
|---|---|
| `auto filtering = Config::getB(BoolKey::proc_filtering)` in `Input::process()` | `std::holds_alternative<input::Filtering>(input_fsm)` |
| `Menu::active` bool in `Input::get()` mouse routing | `std::holds_alternative<input::MenuActive>(input_fsm)` |
| `Config::flip(BoolKey::proc_filtering)` (enter filter mode) | `input_fsm = input::Filtering{ .old_filter = ..., .filter = ... }` |
| `Config::set(BoolKey::proc_filtering, false)` (exit filter mode) | `input_fsm = input::Normal{}` |
| `Menu::active = true` in `process()` when `menuMask` becomes non-empty | `input_fsm = input::MenuActive{}` triggered by first `push()` |
| `Menu::active = false` in `process()` when `menuMask.none()` | `input_fsm = input::Normal{}` triggered by final `pop()` |

---

## Sources

- Codebase analysis: `/Users/mit/Documents/GitHub/btop/src/btop_menu.cpp`, `btop_input.cpp`, `btop_menu.hpp`, `btop_input.hpp`, `btop_state.hpp`
- [Game Programming Patterns: State (Pushdown Automata)](https://gameprogrammingpatterns.com/state.html) — canonical push/pop/replace semantics
- [Implementing State Machines with std::variant](https://khuttun.github.io/2017/02/04/implementing-state-machines-with-std-variant.html) — per-state data in variant alternatives
- [TUI Architecture: Bubble Tea / charmbracelet](https://deepwiki.com/charmbracelet/crush/5.1-tui-architecture-and-appmodel) — dialog priority blocking input, focus FSM
- [Ratatui focus management discussion](https://forum.ratatui.rs/t/focusable-crate-manage-focus-state-for-your-widgets/73) — typed focus state patterns in TUI apps
- [Learning the State Machine Behind (Neo)Vim](https://spin-web.github.io/SPIN2024/assets/preproceedings/SPIN2024-paper9.pdf) — modal input as FSM, mode transitions
- [Game State Manager (C++) — codesmith-fi](https://github.com/codesmith-fi/GameStateManager) — push/pop state stack implementation reference
- `.planning/research/AUTOMATA-ARCHITECTURE.md` — prior feasibility research for btop FSM migration
- `.planning/PROJECT.md` — v1.3 scope, constraints, and key decisions

---
*Feature research for: btop++ v1.3 Menu PDA + Input FSM*
*Researched: 2026-03-02*
