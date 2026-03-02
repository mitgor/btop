# Pitfalls Research

**Domain:** C++ terminal UI — pushdown automaton menu system + input FSM added to existing explicit-FSM architecture (btop++ v1.3)
**Researched:** 2026-03-02
**Confidence:** HIGH — based on direct btop++ source analysis, established C++ FSM literature, and lessons from v1.1/v1.2 milestones

---

## Critical Pitfalls

### Pitfall 1: Variant Self-Modification During std::visit (Dangling Reference UB)

**What goes wrong:**
A visitor lambda takes the current state by reference (e.g., `const MenuFrame& top_frame`), then modifies the stack or PDA variant from inside that lambda. The modification destroys the old variant contents. The reference the lambda holds now points to freed memory. Accessing it is undefined behavior — no compiler warning, no sanitizer catch in all paths, silent data corruption or crash.

This is especially dangerous in the PDA because `process(key)` needs to pop the top frame, which destroys the old frame while still in the call chain derived from inspecting it.

**Why it happens:**
The pattern feels natural: visit the current state, decide what to do based on it, then do it. In simple FSMs where the state never has pointers into itself this is safe. Once the variant is reallocated or destroyed during the visit, any captured references go dangling. The v1.1 App FSM avoided this because `dispatch_event` returns a new `AppStateVar` rather than mutating in place — the same discipline must apply to the PDA.

**How to avoid:**
- Never mutate the PDA stack from inside a `std::visit` call on that same stack's top frame
- Follow the v1.1 pattern exactly: `process(key)` returns a new `MenuPDAVar` (or `std::optional<MenuPDAFrame>`) and the caller applies it after the visit completes
- If the logic requires inspecting the top frame to decide whether to pop, extract the decision as a pure function returning a `PDAAction` enum (Push/Pop/Replace/NoChange), then apply the action outside the visitor
- Run with `-fsanitize=address,undefined` and test rapid menu open/close/open sequences

**Warning signs:**
- Crash only under certain frame sequences (signal choose → signal send → escape)
- Heisenbugs that disappear under ASan (allocator padding changes addresses)
- Valgrind reports use-after-free in `Menu::process`

**Phase to address:**
PDA frame data structure phase (earliest menu design phase). The visitor pattern must be settled before any menu frame is implemented.

---

### Pitfall 2: Static Local State Survives Re-Entry — The Hidden Persistent Menu Bug

**What goes wrong:**
The current `signalChoose`, `optionsMenu`, `helpMenu`, `reniceMenu`, and `mainMenu` functions each carry `static int x`, `static int y`, `static int selected_signal`, `static int page`, `static vector<string> colors_selected`, and similar locals that persist between calls. The existing code resets them when `bg.empty()` (a proxy for "first call of this menu activation"). When these statics are migrated to per-frame struct members, the reset logic must be replicated exactly. If even one field is missed, the menu reopens in a stale visual state — wrong scroll position, wrong selection highlight, wrong page number.

The specific risk: the PDA push-on-show path calls the constructor for the new frame struct, which zero-initializes POD members. But any non-POD member with a non-trivial default constructor (e.g., a `std::string nice_edit`) must be explicitly reset. If a frame struct is ever recycled (stored in a pool or re-pushed from a closed state), old string values survive.

**Why it happens:**
Static locals are reset by condition (`if (bg.empty())`) rather than by construction. When rewriting to struct-based frames, developers frequently translate the condition-based reset into the constructor but miss non-obvious reset points: `signalChoose` resets `selected_signal = -1` in the `bg.empty()` branch, but that branch also rebuilds `bg` — so missing the bg rebuild is equivalent to missing the reset. The two concerns are tangled in the original code.

**How to avoid:**
- Audit every `static` local in every menu function before writing any frame struct. Record each field, its type, its initial value, and when it is reset. This produces the authoritative field list for the frame struct.
- Implement frame struct constructors that unconditionally initialize every field to its "fresh open" value — not conditionally
- Delete the `bg.empty()` reset pattern entirely; replace it with construction semantics: "a new frame is always fresh"
- Treat `bg` (the box drawing string) as a renderable output computed on the first call, not as a state-reset flag
- Write a test for each menu that: opens the menu, interacts with it (changes selected item, page, etc.), closes it, reopens it — and asserts the reopened state matches a fresh open

**Warning signs:**
- Menu reopens on wrong page or with previous selection highlighted
- Signal choose dialog shows last signal number instead of -1 after reopening
- Renice menu shows previous nice value after reopening with a different process selected
- Options menu opens at last scroll position instead of top

**Phase to address:**
The static-locals audit must happen in the first PDA implementation phase, before any frame struct is coded. The test for each menu's fresh-open state must be in the test phase.

---

### Pitfall 3: Dual Menu State — Variant and Bitset Desync During Incremental Migration

**What goes wrong:**
The migration from `menuMask` + `currentMenu` to a PDA stack will be incremental — some menus converted, others not. During the transition, both systems may be live simultaneously. The existing `Menu::process()` reads `menuMask` and `currentMenu` to determine what to display. If the PDA stack says "Options is active" but `menuMask` has been cleared, `Menu::process` sees no active menu and exits. If `menuMask` is set but the PDA stack is empty, `Menu::active` may be set while the PDA has nothing to display.

This is structurally identical to the v1.2 shadow-atomic desync bug (variant and `app_state` atomic diverging), which caused hard-to-reproduce failures before `transition_to()` enforced single-writer invariant. The v1.1 retrospective explicitly flagged this: "shadow atomics create consistency debt."

**Why it happens:**
Incremental migration requires both old and new representations to be maintained simultaneously. Any code path that writes one but not the other creates a window of inconsistency. In the v1.2 case it was the runner error path writing the shadow atomic directly. Here the risk is any call site that calls `menuMask.set()` without also pushing a PDA frame, or that pops a PDA frame without clearing the corresponding `menuMask` bit.

**How to avoid:**
- Apply the single-writer invariant immediately: at the start of the migration, wrap all `menuMask` mutations and `currentMenu` assignments inside a single function (`menu_show()` / `menu_close()`) that updates both representations atomically
- Never let PDA push/pop calls exist without a corresponding `menuMask` update in the same function, or vice versa
- After each menu is migrated, remove the `menuMask.set()` call for that menu and replace it with PDA push only — do not run both in parallel longer than one phase
- Add an assertion at the top of `Menu::process()` that verifies: `menuMask.none() == pda_stack.empty()` during the transition period

**Warning signs:**
- `Menu::active` is false but PDA stack is non-empty (or vice versa)
- Menu appears briefly then immediately closes without user input
- Escape from a menu puts the system in a partially-active state where background renders incorrectly

**Phase to address:**
First migration phase. The wrapper function must be written before the first menu is migrated, not after.

---

### Pitfall 4: Mouse Mapping Ownership Ambiguity Between Menu PDA and Input FSM

**What goes wrong:**
The current code has two `mouse_mappings` maps: `Menu::mouse_mappings` (used when `Menu::active` is true) and `Input::mouse_mappings` (used otherwise). The dispatching in `Input::get()` at line 182 reads:

```cpp
for (const auto& [mapped_key, pos] : (Menu::active ? Menu::mouse_mappings : mouse_mappings))
```

This binary choice will not scale to a PDA stack with multiple frames that each have their own mouse regions. If a modal dialog (SignalSend) is open on top of the Main Menu, the input system must consult only `SignalSend`'s mouse mappings — not the underlying Main Menu's mappings, and not the global input mappings.

If the PDA frame's mouse mappings are merged into a single flat map for efficiency, a key collision between frames (e.g., both Main Menu and Options define a "button1" mapping) silently overwrites one with the other, creating incorrect click targets.

If the Input FSM introduces a `MenuActive` state that holds a reference to the top-frame's mappings, but the PDA stack is modified between key parsing and click dispatch, the reference goes stale.

**Why it happens:**
The original code assumes at most one active menu, making the binary `Menu::active` check sufficient. The PDA introduces a hierarchy — the top frame is what the user sees, lower frames are hidden. The input system needs to be aware of this hierarchy without coupling directly to PDA internals. Developers tend to resolve this by passing a pointer or reference to the top frame's mappings, which creates a lifetime issue when the stack is modified.

**How to avoid:**
- Each PDA frame struct owns its mouse mappings as a value member (`std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings`)
- `Menu::process()` returns (or sets) a single `const std::unordered_map<string, Mouse_loc>*` pointing to the top frame's mappings after each transition
- `Input::get()` reads this pointer exactly once per input event, before any state modification
- The Input FSM's `MenuActive` state stores a pointer to the stable mappings; the pointer is refreshed by `Menu::process()` after every PDA transition, not during input parsing
- No merging of mouse mappings across frames: only the topmost frame's mappings are ever active

**Warning signs:**
- Clicking a button in a modal dialog triggers an action in the underlying menu
- Mouse regions don't update after pushing/popping a PDA frame
- Click coordinates map to the wrong action after terminal resize with menu open

**Phase to address:**
Input FSM design phase. The mappings ownership model must be settled before implementing any Input FSM state struct.

---

### Pitfall 5: Input FSM `proc_filtering` State Encoded in Config, Not FSM

**What goes wrong:**
The current `Input::process()` checks `Config::getB(BoolKey::proc_filtering)` at the top to decide whether to enter filter-editing logic. This boolean lives in the config system, not in an FSM state. When the Input FSM is introduced with a `Filtering` state, there will be two representations of the same fact: the FSM state and the config value. If the FSM transitions to `Filtering` but `proc_filtering` is not set, or vice versa, the behavior diverges.

Further: `Input::process()` calls `Config::set(BoolKey::proc_filtering, false)` to exit filtering mode. After the Input FSM exists, this write must also trigger an FSM transition to `Normal`. If the write happens but the transition does not (or happens in the wrong order), the FSM is in `Filtering` state while the config says `false` — the next key press will be handled by `Normal` handlers but the filtering UI is still visible.

**Why it happens:**
The config system is the original source of truth for all stateful toggles in btop. Introducing a separate FSM that tracks the same fact creates dual-write responsibility. Developers tend to add FSM transitions in some code paths but miss others (keyboard shortcut, mouse click, escape, programmatic config set). The v1.2 milestone had the same issue: `runner_var` was a dead variant because all writes went through the shadow atomic, leaving the variant stale.

**How to avoid:**
- Decide before coding: is `proc_filtering` a config value or an FSM state? Recommendation: make it an FSM state (`InputFSM::Filtering`) that is the authoritative source; remove the `proc_filtering` config key or make it a read-only view of the FSM state at save time
- All code paths that previously called `Config::set(BoolKey::proc_filtering, true/false)` must be replaced with FSM transitions
- Grep for every read of `proc_filtering` and verify each one is replaced by FSM state check
- Run the full test suite after migration to verify all entry/exit points to filtering mode are correct

**Warning signs:**
- Entering filter mode and pressing escape leaves the filter text box visible but the FSM in `Normal` state
- Typing characters after pressing 'f' does nothing (FSM thinks it's in `Normal` state)
- `proc_filtering` config file entry is written as `true` after program exit despite filter not being active

**Phase to address:**
Input FSM implementation phase. The config key removal or aliasing must happen in the same phase as the FSM introduction, not deferred.

---

### Pitfall 6: Existing App FSM Resize Handling Calls `Menu::process()` Without PDA Awareness

**What goes wrong:**
The `on_enter(state::Resizing&, ...)` function in `btop.cpp` line 1009 calls:

```cpp
if (Menu::active) Menu::process();
```

This call is designed to re-render the current menu after a resize. It works with the current bitset model because `menuMask` and `currentMenu` survive the resize. With a PDA, the same call must re-render the top frame. But the PDA frame structs store absolute screen coordinates (`x`, `y`, `height`, `width`) computed at push time from `Term::width` and `Term::height`. After a resize, these stored coordinates are stale — they reference a terminal size that no longer exists.

If the frame struct is re-rendered using stale coordinates but the frame is not reconstructed (re-pushed), the menu appears at wrong coordinates or overflows the terminal boundary.

If the frame is reconstructed on resize (coordinates recomputed), the frame's non-coordinate state (current page, current selection, signal being chosen) must be preserved. The existing static-local approach handles this naturally because the statics survive. Frame struct-based PDA must explicitly separate "layout state" (coordinates, renderable strings) from "interaction state" (selection, page, entered values).

**Why it happens:**
Coordinates are convenient to precompute and store in the frame because they are used on every render. But computing them at push time means they are only valid for the terminal size at the time of the push. The existing code treats `redraw = true` on resize as a cue to recompute, and `bg.empty()` as the trigger to reinitialize all layout state. PDA frames will not have a `bg.empty()` signal by design.

**How to avoid:**
- Separate PDA frame data into two categories in the struct:
  - `layout`: `x`, `y`, `width`, `height`, `bg` (the rendered box string) — all derived from terminal size
  - `interaction`: selection index, page number, entered text, chosen signal — user interaction state
- On resize, invalidate only `layout` fields (e.g., set `bg.clear()`, zero coordinates) while preserving `interaction` fields
- Give each frame struct an `invalidate_layout()` method that `Menu::process()` calls when it detects a resize
- Write a test that: opens SignalChoose, resizes terminal, verifies signal list redraws at correct coordinates but `selected_signal` is preserved

**Warning signs:**
- Menu appears at top-left corner (0,0) after resize
- Menu overflows terminal boundary after resize to smaller terminal
- Selection resets to default after resize
- Resize while menu is open causes double-draw artifacts

**Phase to address:**
PDA frame struct design phase. The layout/interaction separation must be in the initial struct definition, not added as a retrofit.

---

### Pitfall 7: `Menu::show()` Called from Input::process() Creates a Re-Entrant Call Chain

**What goes wrong:**
The existing call chain is: main loop → `Input::process(key)` → `Menu::show(Menu::Menus::SignalSend, SIGTERM)` → `menuMask.set(menu)` → `Menu::process()`. This is a re-entrant call into the menu system from inside the input processor. With a PDA, `Menu::show()` would push a frame onto the PDA stack. But `Input::process()` is called when `Menu::active` is false. If the Input FSM has a `Normal` state, the `Menu::show()` call transitions the menu PDA and sets `Menu::active = true`. On the next loop iteration, `Menu::process()` runs. This is coherent.

The dangerous case: if `Menu::show()` is called from inside `Menu::process()` (e.g., Main Menu choosing Options pushes the Options frame directly via `menuMask.set(Menus::Options)` in `mainMenu()`). With a PDA, this becomes a push-inside-process call. If `Menu::process()` dispatches to the top frame via `std::visit`, and that visitor calls `Menu::show()` which pushes a new frame, the stack is modified during iteration. If `std::visit` holds a reference to the old top frame, it is now dangling (see Pitfall 1).

**Why it happens:**
The existing `Switch` return code from `menuFunc` triggers `Menu::process()` recursive call (lines 1866-1872). This is an undocumented re-entrant call pattern. The `mainMenu` function calls `menuMask.set(Menus::Options)` and `currentMenu = Menus::Options` directly (lines 1229-1234), then returns `Changed`. The `process()` function's response to `Switch` reinvokes `process()`. With a PDA, this must become a push action returned from the frame handler, applied after the visitor completes.

**How to avoid:**
- Frame handlers must never push/pop the PDA stack directly; they return a `PDAAction` (Push/Pop/Replace/NoChange) and the caller applies it
- The `Switch` return code from v1 must be renamed to `Push(next_frame)` or `Replace(next_frame)` that carries the new frame as a value
- `Menu::show()` must be called only from outside `Menu::process()` — from `Input::process()` or the main loop — never from inside a frame handler
- In-menu navigation (Main → Options) must return `Push(OptionsFrame{})` not call `Menu::show()` directly

**Warning signs:**
- Opening Options from Main Menu crashes or shows blank overlay
- Pressing Options in Main Menu has no effect
- The PDA stack has two frames that both claim to be the current menu simultaneously

**Phase to address:**
PDA action design phase (concurrent with frame struct design). The action return type must be established before any frame handler is implemented.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Keep `menuMask` alive alongside PDA during migration | No big-bang change, safe fallback | Desync between bitset and PDA grows as more menus migrate; each desync is a latent bug | Only for a single phase; remove `menuMask` completely in the phase that completes the last menu migration |
| Store mouse mappings in a global flat map instead of per-frame | No change to `Input::get()` dispatch | Frame boundaries are invisible to the input system; collision between frame keys is silently wrong | Never — per-frame ownership is required for PDA correctness |
| Use `Config::proc_filtering` as the FSM's source of truth during migration | Avoids rewriting all filtering entry points at once | Two sources of truth; any path that writes one and not the other leaves them desynced | Acceptable for at most one phase, with a specific plan to remove the config key |
| Leave `static` locals in unconverted menu functions | Reduced scope of each PR | Static locals persist across menu open/close cycles; incompatible with per-frame construction semantics | Only while that menu has not yet been migrated; must be fully removed when the frame is introduced |
| Compute coordinates at push time and never invalidate | Simpler push logic | Stale coordinates on resize; menu renders at wrong position | Never — resize must invalidate layout state |

## Integration Gotchas

Common mistakes when connecting the new PDA + Input FSM to the existing btop systems.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| App FSM Resizing entry action | Calling `Menu::process()` without invalidating PDA frame layout | `on_enter(Resizing)` calls `menu_pda.invalidate_layout()` before `Menu::process()` |
| Runner `pause_output` flag | Set in `Menu::process()` but cleared in multiple paths; PDA pop must clear it | PDA pop action must always clear `pause_output` regardless of which frame was popped |
| `Global::overlay` string | Currently set directly in each menu function; with PDA, the active frame owns the overlay | Only `Menu::process()` writes `Global::overlay`; frame handlers return rendered strings, not write globals |
| `Input::get()` mouse dispatch | Reading `Menu::mouse_mappings` directly; must read top-frame mappings | `Input::get()` reads a stable pointer set by `Menu::process()` after each PDA transition |
| `Runner::run("overlay")` call | Called with frame-specific logic; PDA must know whether to run overlay or all | Frame handlers return a render scope (`overlay` or `all`); `Menu::process()` calls `Runner::run(scope)` |
| `Input::process()` calling `Menu::show()` | After Input FSM is introduced, `Menu::show()` must also transition the Input FSM | `Menu::show()` → push PDA frame AND transition Input FSM to `MenuActive` atomically |

## Performance Traps

Patterns that create measurable overhead in the menu-active render path.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Rebuilding full options menu string on every keypress (not just on `Changed`) | Options menu renders lag on slow terminals | Only rebuild `Global::overlay` when frame handler returns `Changed`, not on `NoChange` | Always — the options menu string is large (~4KB) |
| `std::vector` for PDA stack with reserve-on-push | Reallocation invalidates all pointers to stack elements | Use a fixed-capacity stack (max depth is known: 8 menus) or `std::array<std::optional<Frame>>` | When OptionsMenu is pushed on top of MainMenu and the vector reallocates |
| Per-keypress `mouse_mappings.clear()` and rebuild | Mouse regions flicker during rapid input | Only rebuild mouse mappings on `redraw`, not on `NoChange` returns | Fast input sequences (keyboard repeat) |
| Copying frame structs on push | Unnecessary copy of `bg` string (~2KB for Options) | Use move semantics: `push(std::move(frame))` | Options menu push |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **PDA push/pop:** Often missing — verify that popping the last frame sets `Menu::active = false` and clears `Global::overlay` immediately, not on next loop iteration
- [ ] **Static local audit:** Often missing fields — verify every `static` local in all 8 menu functions is accounted for in a frame struct member before removing it
- [ ] **Resize with menu open:** Often skipped in testing — verify each menu renders correctly after terminal resize with no selection reset
- [ ] **Input FSM Filtering exit paths:** Often missing the mouse_click exit — verify that `mouse_click` while filtering exits filtering mode (existing code path line 319 in `btop_input.cpp`)
- [ ] **Re-entry clean state:** Often misses non-obvious resets — verify that reopening SignalChoose after a previous selection shows `selected_signal = -1` not the previous value
- [ ] **`pause_output` cleared on all pop paths:** Often missed for error pop paths — verify that `Runner::pause_output` is always false after any PDA empty state
- [ ] **Mouse mappings updated after resize:** Often missing — verify that clicking after a resize with menu open uses new screen coordinates
- [ ] **Config save on Options close:** Often broken by FSM changes — verify that closing Options menu writes config to disk (existing behavior via `optionsMenu` Closed handler)

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Variant self-modification UB | HIGH | Run ASan — may not catch; run Valgrind with `--tool=memcheck`; restructure visitor to extract action type, apply outside visit; add regression test for the sequence that triggered it |
| Static local state not reset | LOW | Add test for fresh-open invariant; patch frame struct constructor to explicitly set all fields; run test suite |
| Bitset/PDA desync | MEDIUM | Introduce `assert(menuMask.none() == pda_stack.empty())` in `Menu::process()` to catch immediately; trace which code path set one without the other; add to single-writer wrapper |
| Mouse mapping ownership confusion | MEDIUM | Add click-region test with deliberate overlap; trace which mapping table `Input::get()` reads; switch to per-frame ownership pattern |
| Input FSM / proc_filtering desync | MEDIUM | Grep for all `proc_filtering` writes; ensure each one also transitions the Input FSM; add test that both agree after every mode change |
| Resize with stale coordinates | LOW | Implement `invalidate_layout()` on frame structs; call from `on_enter(Resizing)`; test by resizing terminal while each menu is open |
| Re-entrant push during std::visit | HIGH | If crash: use `PDAAction` return-value pattern instead of direct push; add test that covers Main→Options in-menu navigation |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Variant self-modification UB | PDA frame data structure design (Phase 1) | Visitor pattern established with PDAAction return type before any frame impl |
| Static local state not reset | Static locals audit (Phase 1, before coding) | Each menu's fresh-open invariant documented and tested |
| Bitset/PDA desync | First migration phase | `menuMask`/PDA wrapper function written; assertion added to `Menu::process()` |
| Mouse mapping ambiguity | Input FSM design phase | Per-frame ownership model established; `Input::get()` reads stable pointer |
| `proc_filtering` dual-truth | Input FSM implementation phase | Config key removed or aliased; all entry/exit paths go through FSM transitions |
| Resize with stale coordinates | PDA frame struct design (layout/interaction split) | Resize test for each menu verifies coordinates and preserves selection |
| Re-entrant push during process | PDA action design phase | Frame handlers return PDAAction; no direct push calls inside frame code |
| App FSM integration (resize) | First phase that touches `on_enter(Resizing)` | `Menu::process()` resize call works correctly with PDA stack |

## Sources

- btop++ source code analysis: `btop_menu.cpp`, `btop_input.cpp`, `btop_state.hpp`, `btop_events.hpp`, `btop.cpp` (direct codebase inspection) — HIGH confidence
- v1.1 Retrospective (`.planning/RETROSPECTIVE.md`): shadow atomic consistency debt, runner_var dead code, incremental migration lessons — HIGH confidence
- v1.2 Retrospective: single-writer invariant enforcement as solution to desync bugs — HIGH confidence
- [Finite State Machines with std::variant — C++ Stories (2023)](https://www.cppstories.com/2023/finite-state-machines-variant-cpp/): dangling reference from variant self-modification — HIGH confidence
- [Space Game: std::variant-Based State Machine by Example — C++ Stories (2019)](https://www.cppstories.com/2019/06/fsm-variant-game/): visitor self-assignment pattern; optional return for conditional transitions — HIGH confidence
- [std::visit — cppreference.com](https://en.cppreference.com/w/cpp/utility/variant/visit): behavior when variant is modified during visitation — HIGH confidence (official)
- [QP State Machine: State-Local Storage Pattern](https://www.state-machine.com/doc/Pattern_SLS.pdf): per-state data ownership, separation of layout from interaction state — MEDIUM confidence

---
*Pitfalls research for: btop++ v1.3 — Menu PDA + Input FSM*
*Researched: 2026-03-02*
