# Phase 22: PDA Dispatch - Research

**Researched:** 2026-03-02
**Domain:** C++ std::visit dispatch on variant-based pushdown automaton; elimination of bitset/int/vector dispatch in a menu system
**Confidence:** HIGH

## Summary

Phase 22 replaces the legacy three-variable dispatch mechanism (menuMask bitset + currentMenu int + menuFunc vector) with std::visit on the PDA's top frame. This is the central phase of the v1.3 milestone: it makes the PDA the sole dispatch authority for all menu rendering.

Phase 21 established the coexistence architecture: every menu function already reads/writes frame struct members via `std::get<XxxFrame>(pda.top())`, and single-writer wrappers keep the PDA and legacy dispatch in sync. Phase 22's job is to flip the dispatch: `process()` will call `std::visit(visitor, pda.top())` instead of `menuFunc.at(currentMenu)(key)`, and all return codes (Closed/Changed/NoChange/Switch) will be replaced by PDAAction values (Push/Pop/Replace/NoChange) that the caller applies after the visitor completes.

The four success criteria map to specific code transformations: (1) Delete menuMask/currentMenu/menuFunc and rewrite process() to use std::visit. (2) Main-to-Options and Main-to-Help transitions already use `pda.replace()` in Phase 21; Phase 22 makes these the canonical path by eliminating the Switch return code and its re-entrant `process()` call. (3) bg lifecycle is already tied to PDA (set_bg/clear_bg); Phase 22 formalizes it: `pda.bg().empty()` triggers capture on first redraw, `pda.clear_bg()` happens on pop when depth reaches 0. (4) The Switch code and its recursive `process()` call are eliminated entirely; handlers return PDAAction::Replace, and process() applies it non-recursively.

**Primary recommendation:** Build a visitor struct with `operator()(FrameType&, std::string_view key) -> PDAAction` overloads for all 8 frame types, each wrapping the existing static function. Rewrite `process()` as a non-recursive loop that applies PDAActions. Delete menuMask, currentMenu, menuFunc, and the Menus enum. Keep Menu::active as-is (Phase 23 removes it when InputFSM takes over).

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PDA-04 | PDA dispatch replaces menuMask bitset + currentMenu int + menuFunc dispatch vector | The visitor pattern with std::visit on MenuFrameVar enables compile-time exhaustive dispatch to all 8 frame types. menuMask/currentMenu/menuFunc become dead code once process() dispatches via visitor. The existing `std::get<XxxFrame>(pda.top())` in each function is already the correct frame retrieval pattern. |
| PDA-06 | replace() operation handles Main-to-Options and Main-to-Help transitions (ESC returns to Normal, not Main) | Phase 21 already calls `pda.replace(menu::OptionsFrame{})` and `pda.replace(menu::HelpFrame{})` in mainMenu. Phase 22 makes this the canonical path: the handler returns `PDAAction::Replace` with a payload frame, and process() applies it. ESC from Options/Help returns Closed->Pop, which pops the replacement frame, leaving the stack empty (Normal state). |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| std::visit | C++17 (C++23 project) | Dispatch on MenuFrameVar to call the correct menu handler | Compile-time exhaustive matching; adding a new frame type without a handler is a compile error |
| std::variant | C++17 (C++23 project) | MenuFrameVar already defined in btop_menu_pda.hpp | Already in use from Phase 20 |
| std::stack | C++98 (C++23 project) | MenuPDA's internal stack | Already in use from Phase 20 |
| GTest | v1.17.0 | Unit tests for PDA dispatch behavior | Already fetched via FetchContent in tests/CMakeLists.txt |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::string_view | C++17 | Key parameter type for process() and visitor | Already used in process() signature |
| fmt::format | project dep | String formatting in menu rendering | Already in use throughout btop_menu.cpp |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Visitor struct with operator() | Generic lambda with if-constexpr | Visitor struct is clearer for 8 overloads; lambda would be unreadable at this size |
| PDAAction return value | Direct PDA mutation inside handler | Direct mutation causes UB with variant self-modification during visit; PDAAction defers mutation to the caller |
| Non-recursive process() loop | Keep recursive process() | Recursive call is the source of the Switch re-entrant pattern; loop eliminates unbounded recursion risk |

## Architecture Patterns

### Recommended Project Structure
```
src/
  btop_menu_pda.hpp      # MODIFIED: add PDAResult struct (PDAAction + optional payload frame)
  btop_menu.hpp          # MODIFIED: remove menuMask, Menus enum; keep active, output, mouse_mappings, show(), process()
  btop_menu.cpp          # MODIFIED: visitor struct, rewritten process(), adapted show(), delete menuFunc/currentMenu
tests/
  test_menu_pda.cpp      # MODIFIED: add PDAAction dispatch tests
```

### Pattern 1: PDAResult Return Type

**What:** A struct that combines a PDAAction with an optional payload frame for Push/Replace actions.

**Why needed:** When mainMenu selects "Options", it returns PDAAction::Replace along with a new OptionsFrame{}. The current PDAAction enum alone cannot carry the replacement frame. A result struct solves this.

**Implementation:**
```cpp
// In btop_menu_pda.hpp
struct PDAResult {
    PDAAction action{PDAAction::NoChange};
    std::optional<MenuFrameVar> next_frame{};
    bool needs_redraw{false};
};
```

**Alternative (simpler):** Since the handler already has access to `pda` as a file-scope variable and Phase 21 already calls `pda.replace()` directly inside mainMenu, we could keep that pattern and just return a simpler action. BUT this violates success criterion 4 ("all frame transitions return PDAAction values applied by the caller after the visitor completes"). So either:
- Option A: PDAResult struct with optional frame payload (clean separation)
- Option B: Handler sets a "pending frame" on the PDA before returning, and process() applies it

**Recommendation:** Option A (PDAResult) is cleaner and matches the architectural intent. The mainMenu handler returns `{PDAAction::Replace, menu::OptionsFrame{}}` and process() calls `pda.replace(std::move(*result.next_frame))`.

### Pattern 2: Menu Visitor Struct

**What:** A callable struct with 8 `operator()` overloads that dispatch to the existing static handler functions.

**When to use:** Called from process() via `std::visit(MenuVisitor{key}, pda.top())`.

**Implementation:**
```cpp
struct MenuVisitor {
    std::string_view key;

    PDAResult operator()(menu::MainFrame& frame) const {
        return mainMenu(key, frame);
    }
    PDAResult operator()(menu::OptionsFrame& frame) const {
        return optionsMenu(key, frame);
    }
    PDAResult operator()(menu::HelpFrame& frame) const {
        return helpMenu(key, frame);
    }
    PDAResult operator()(menu::SizeErrorFrame& frame) const {
        return sizeError(key, frame);
    }
    PDAResult operator()(menu::SignalChooseFrame& frame) const {
        return signalChoose(key, frame);
    }
    PDAResult operator()(menu::SignalSendFrame& frame) const {
        return signalSend(key, frame);
    }
    PDAResult operator()(menu::SignalReturnFrame& frame) const {
        return signalReturn(key, frame);
    }
    PDAResult operator()(menu::ReniceFrame& frame) const {
        return reniceMenu(key, frame);
    }
};
```

**Key insight:** Each static function already does `auto& frame = std::get<XxxFrame>(pda.top())` -- after this change, the frame is passed as a parameter, eliminating the `std::get` call inside each function. The function signature changes from `int(const string& key)` to `PDAResult(std::string_view key, XxxFrame& frame)`.

### Pattern 3: Non-Recursive process() Loop

**What:** Replace the recursive `process()` call pattern (Closed -> process(), Switch -> process()) with a loop that applies PDAActions.

**Why needed:** The current process() recursively calls itself when a menu closes (to check if another menu is pending) or when Switch occurs (to start the new menu). This recursion is the source of the re-entrant call pattern that success criterion 4 requires eliminating.

**Implementation:**
```cpp
void process(const std::string_view key) {
    bool first_call = true;
    std::string_view current_key = key;

    while (true) {
        // Empty stack -> no menu active
        if (pda.empty()) {
            Menu::active = false;
            Global::overlay.clear();
            Global::overlay.shrink_to_fit();
            Runner::pause_output.store(false);
            pda.clear_bg();
            mouse_mappings.clear();
            Runner::run("all", true, true);
            return;
        }

        Menu::active = true;

        // SizeError override check (terminal too small)
        if (first_call) {
            bool needs_size_check = std::visit([](const auto& f) -> bool {
                using T = std::decay_t<decltype(f)>;
                return std::is_same_v<T, menu::MainFrame>
                    || std::is_same_v<T, menu::OptionsFrame>
                    || std::is_same_v<T, menu::HelpFrame>
                    || std::is_same_v<T, menu::SignalChooseFrame>;
            }, pda.top());

            if ((needs_size_check && (Term::width < 80 || Term::height < 24))
                || (Term::width < 50 || Term::height < 20)) {
                while (!pda.empty()) pda.pop();
                pda.push(menu::SizeErrorFrame{});
                redraw = true;
            }
        }

        if (first_call && redraw) {
            redraw = true;  // ensure redraw on first entry
        }

        // Dispatch to active menu handler
        auto result = std::visit(MenuVisitor{current_key}, pda.top());

        // Apply PDAAction
        switch (result.action) {
            case menu::PDAAction::NoChange:
                if (result.needs_redraw) {
                    redraw = false;
                    Runner::run("all", true, true);
                }
                return;

            case menu::PDAAction::Pop:
                pda.pop();
                mouse_mappings.clear();
                pda.clear_bg();
                Runner::pause_output.store(false);
                // Loop continues: check if stack is now empty or another frame needs dispatch
                current_key = "";
                first_call = false;
                redraw = true;
                continue;

            case menu::PDAAction::Replace:
                assert(result.next_frame.has_value());
                pda.replace(std::move(*result.next_frame));
                pda.clear_bg();
                Runner::pause_output.store(false);
                mouse_mappings.clear();
                current_key = "";
                first_call = false;
                redraw = true;
                continue;

            case menu::PDAAction::Push:
                assert(result.next_frame.has_value());
                pda.push(std::move(*result.next_frame));
                current_key = "";
                first_call = false;
                redraw = true;
                continue;
        }

        // Changed result (menu rendered new content)
        Runner::run("overlay");
        return;
    }
}
```

**Critical insight:** The loop replaces two separate recursive calls: (1) the `Closed -> process()` path (now `Pop -> continue`) and (2) the `Switch -> process()` path (now `Replace -> continue`). This eliminates unbounded recursion.

### Pattern 4: show() Simplification

**What:** `Menu::show()` pushes a frame onto the PDA and calls process(). The Menus enum is no longer needed for dispatch.

**After:**
```cpp
void show(int menu, int signal) {
    // menu parameter kept for backward compatibility with btop_input.cpp call sites
    // These will be cleaned up in Phase 23/25
    signalToSend = signal;
    switch (menu) {
        case 7: pda.push(menu::MainFrame{}); break;    // Main
        case 4: pda.push(menu::OptionsFrame{}); break;  // Options
        case 5: pda.push(menu::HelpFrame{}); break;      // Help
        case 0: pda.push(menu::SizeErrorFrame{}); break; // SizeError
        case 1: pda.push(menu::SignalChooseFrame{}); break; // SignalChoose
        case 2: pda.push(menu::SignalSendFrame{}); break;   // SignalSend
        case 3: pda.push(menu::SignalReturnFrame{}); break;  // SignalReturn
        case 6: pda.push(menu::ReniceFrame{}); break;       // Renice
    }
    process();
}
```

**Better approach:** Keep the Menus enum values but make them map to frame construction only. The enum is used by external callers (btop_input.cpp) and removing it changes the public interface. Keep it in the header during Phase 22; Phase 25 cleanup can replace it with a typed API.

### Pattern 5: SignalChoose/SignalSend "Post-Pop Push" Pattern

**What:** `signalChoose()` and `signalSend()` set `menuMask.set(SignalReturn)` before returning Closed, creating a "close current and immediately open another" pattern.

**PDA translation:** The handler returns `PDAAction::Replace` with a `SignalReturnFrame{}` (not Pop then Push). This is semantically correct: the signal menu is replaced by the error result dialog. When the user dismisses SignalReturn, it returns Pop, clearing the stack entirely.

**However, for signalChoose where kill succeeds:** The handler returns Pop (Closed without error). Only when `signalKillRet != 0` does it return Replace with SignalReturnFrame.

**Implementation in handler:**
```cpp
PDAResult signalChoose(std::string_view key, menu::SignalChooseFrame& frame) {
    // ... input handling ...
    if (is_in(key, "enter", "space") && selected_signal >= 0) {
        signalKillRet = 0;
        if (s_pid < 1) {
            signalKillRet = ESRCH;
        } else if (kill(s_pid, selected_signal) != 0) {
            signalKillRet = errno;
        }
        if (signalKillRet != 0) {
            return {PDAAction::Replace, menu::SignalReturnFrame{}};
        }
        return {PDAAction::Pop};
    }
    // ...
}
```

**Key insight:** This eliminates the `menuMask.set(SignalReturn)` side-effect inside the handler. The handler now returns intent (Replace with new frame) rather than mutating shared state.

### Pattern 6: bg Lifecycle Formalization

**What:** bg is captured on first redraw when stack is non-empty, and cleared on final pop (stack empty).

**Current state (Phase 21):** `pda.set_bg()` is called inside each menu function's `if (redraw)` block. `pda.clear_bg()` is called in process() on Closed and Switch paths.

**Phase 22 change:** The bg lifecycle is now solely controlled by process() loop:
- On Pop: `pda.clear_bg()` always (the next frame will recompute bg on its own redraw)
- On Replace: `pda.clear_bg()` (the replacement frame recomputes)
- On empty stack: bg is already clear from the Pop that emptied it
- Individual handlers still call `pda.set_bg()` in their redraw blocks (unchanged)

**Subtle point:** `optionsMenu` checks `if (pda.bg().empty()) Theme::updateThemes()` -- this uses bg emptiness as a signal for first-time entry. After Phase 22, bg will be cleared on Replace, so re-entering Options via Main->Options will trigger Theme::updateThemes() correctly.

### Anti-Patterns to Avoid

- **Mutating the PDA stack inside std::visit:** Calling `pda.push()`, `pda.pop()`, or `pda.replace()` while the visitor lambda is still executing on a reference to the top frame causes undefined behavior (the reference may be invalidated). ALL mutations must happen AFTER the visitor returns.

- **Keeping the recursive process() call:** The recursion is the mechanism for Closed and Switch handling. It MUST be replaced with a loop. Keeping it means the Switch re-entrant pattern survives, violating success criterion 4.

- **Deleting Menu::active in Phase 22:** Menu::active is read by the runner thread (btop.cpp line 771, 906, 1009, 1544, 1572) and by btop_input.cpp (line 182). Phase 23's InputFSM will take over this role. Phase 22 must continue to set/clear Menu::active to maintain the external interface.

- **Removing the Menus enum from the header:** External callers (btop_input.cpp lines 234, 238, 242, 259, 283, 504, 510, 516) use `Menu::Menus::Main`, `Menu::Menus::Help`, etc. to call `Menu::show()`. The enum must remain in the header until Phase 25 cleanup.

- **Forgetting the optionsMenu self-recursive call:** optionsMenu calls itself recursively on theme_refresh (line 1657: `optionsMenu("")`). This must be adapted: the handler should set redraw=true and return NoChange (or a special action) rather than calling itself. Alternatively, leave the self-call since it does not interact with the PDA stack (it only re-renders the same frame).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Exhaustive dispatch over 8 menu types | if-else chain or switch on type index | std::visit with visitor struct | Compiler enforces exhaustiveness; adding a 9th frame type without a handler is a compile error |
| Frame payload in return value | Out-parameter or global "pending frame" | std::optional<MenuFrameVar> in PDAResult | Type-safe, no hidden state, caller applies it explicitly |
| Non-recursive re-dispatch | Recursive process() calls | while(true) loop with continue | Bounded stack usage, no re-entrancy bugs |

**Key insight:** The entire menuFunc vector (8 function references) + menuMask (8-bit bitset) + currentMenu (int index) + Menus enum (index-to-function mapping) is replaced by ONE std::visit call. This is not just a refactor -- it eliminates an entire category of desync bugs where menuMask says one thing and currentMenu says another.

## Common Pitfalls

### Pitfall 1: Variant Self-Modification During Visit
**What goes wrong:** A handler calls `pda.pop()` or `pda.replace()` while still holding a reference to the top frame obtained via std::visit. The stack modification invalidates the reference, causing UB.
**Why it happens:** Phase 21 already has `pda.replace()` calls inside mainMenu (lines 1237, 1242). These must be removed in Phase 22 since process() will apply the action.
**How to avoid:** Handlers return PDAResult with the desired action and payload. process() applies it AFTER the visitor returns. grep for `pda.push`, `pda.pop`, `pda.replace` inside handler functions to verify none remain.
**Warning signs:** ASan/UBSan reports on stack-use-after-return or heap-use-after-free after menu transitions.

### Pitfall 2: optionsMenu Self-Recursion on theme_refresh
**What goes wrong:** optionsMenu (line 1657) calls `optionsMenu("")` recursively to force an immediate re-render after changing the theme. If the function signature changes to receive a frame reference, this self-call still works (it re-renders the same frame). But it changes the return value semantics: the outer call now returns PDAResult from the inner call.
**Why it happens:** Theme refresh requires immediate re-render with new colors before returning to the caller.
**How to avoid:** The simplest approach: let optionsMenu call itself with the same frame reference. The inner call returns a PDAResult (likely NoChange with needs_redraw), which the outer call can pass through. This is not a PDA mutation -- it is rendering the same frame again.
**Warning signs:** Double-rendering or stale theme colors after changing color_theme in Options.

### Pitfall 3: signalChoose/signalSend MenuMask Mutation Removal
**What goes wrong:** Lines 1021, 1025 (signalChoose) and 1143 (signalSend) currently call `menuMask.set(SignalReturn)` before returning Closed. If these are simply deleted without replacing the logic, the SignalReturn dialog never appears after a failed kill().
**Why it happens:** The handler communicated "open SignalReturn next" by setting a bit in menuMask. In Phase 22, it must communicate this via PDAResult::Replace with SignalReturnFrame.
**How to avoid:** Replace `menuMask.set(SignalReturn); return Closed;` with `return {PDAAction::Replace, menu::SignalReturnFrame{}}`. For the success path (no error), return `{PDAAction::Pop}`.
**Warning signs:** Failed kill() signals not showing the error dialog. Test manually: send signal to PID 0 or a non-existent PID.

### Pitfall 4: SizeError Override Path
**What goes wrong:** process() currently clears all menus via `menu_clear_all()` and opens SizeError when the terminal is too small. In the new loop-based process(), this must clear the PDA stack and push a SizeErrorFrame before the dispatch.
**Why it happens:** SizeError is a meta-override that cancels whatever was being opened.
**How to avoid:** The SizeError check should happen at the top of the loop, before dispatching to the visitor. If the terminal is too small, clear the stack and push SizeErrorFrame, then let the loop dispatch to it normally.
**Warning signs:** Menus appearing in too-small terminals, or SizeError not appearing.

### Pitfall 5: mainMenu's menuMask.set() and currentMenu Assignments
**What goes wrong:** Lines 1235-1236 (`menuMask.set(Menus::Options); currentMenu = Menus::Options;`) and 1240-1241 (`menuMask.set(Menus::Help); currentMenu = Menus::Help;`) set legacy state before returning Switch. In Phase 22, these are dead code since menuMask and currentMenu are deleted. But the `pda.replace()` calls on lines 1237 and 1242 are ALSO problematic (Pitfall 1: mutation during visit).
**Why it happens:** Phase 21 added the pda.replace() calls alongside the legacy mutations for dual-write safety.
**How to avoid:** Delete ALL of lines 1235-1242 (menuMask.set, currentMenu assignment, AND pda.replace). Replace with `return {PDAAction::Replace, menu::OptionsFrame{}}` or `return {PDAAction::Replace, menu::HelpFrame{}}`.
**Warning signs:** Compile errors on deleted menuMask/currentMenu (good -- they confirm deletion is complete).

### Pitfall 6: Mouse Mappings Source in btop_input.cpp
**What goes wrong:** btop_input.cpp line 182 reads `Menu::mouse_mappings` to resolve mouse clicks when a menu is active. Currently, each handler writes to the file-scope `Menu::mouse_mappings`. But Phase 20 added per-frame `frame.mouse_mappings`. The handlers currently write to FRAME mouse_mappings (e.g., line 1079, 1268) while btop_input.cpp reads the GLOBAL mouse_mappings.
**Why it happens:** Phase 21 migrated the handlers to use `frame.mouse_mappings` but the global `Menu::mouse_mappings` is still what btop_input.cpp reads.
**How to avoid:** In Phase 22, process() must copy the active frame's mouse_mappings to the global `Menu::mouse_mappings` after each dispatch. OR, Phase 22 could make `Menu::mouse_mappings` a reference/pointer to the top frame's mappings. The simplest approach: after std::visit dispatch, copy `frame.mouse_mappings` to `Menu::mouse_mappings` (this is a swap/move, not expensive for a small map).
**Warning signs:** Mouse clicks not working on menu items. Verify by clicking on Options/Help/Quit in Main menu and on signal entries in SignalChoose.

### Pitfall 7: Runner::pause_output Clearing on Pop
**What goes wrong:** Runner::pause_output must be cleared when the stack becomes empty (all menus closed). If Pop leaves frames on the stack, pause_output should remain set.
**Why it happens:** The current code clears pause_output on every Closed path. But in a PDA, Closed (Pop) might reveal a frame underneath (e.g., SizeError pushed over Main).
**How to avoid:** Clear pause_output only when the stack becomes empty after a Pop. The loop already handles this: if the stack is empty, it enters the "no menu active" cleanup which clears pause_output.
**Warning signs:** Background processes resuming while a menu is still open, or freezing after closing a menu.

## Code Examples

### PDAResult Definition
```cpp
// In btop_menu_pda.hpp, after PDAAction enum

struct PDAResult {
    PDAAction action{PDAAction::NoChange};
    std::optional<MenuFrameVar> next_frame{};  // payload for Push/Replace
    bool needs_redraw{false};                   // signal that Runner::run("all") is needed
};
```

### Visitor Struct
```cpp
// In btop_menu.cpp, after all static handler functions

struct MenuVisitor {
    std::string_view key;

    PDAResult operator()(menu::MainFrame& frame) const { return mainMenu(key, frame); }
    PDAResult operator()(menu::OptionsFrame& frame) const { return optionsMenu(key, frame); }
    PDAResult operator()(menu::HelpFrame& frame) const { return helpMenu(key, frame); }
    PDAResult operator()(menu::SizeErrorFrame& frame) const { return sizeError(key, frame); }
    PDAResult operator()(menu::SignalChooseFrame& frame) const { return signalChoose(key, frame); }
    PDAResult operator()(menu::SignalSendFrame& frame) const { return signalSend(key, frame); }
    PDAResult operator()(menu::SignalReturnFrame& frame) const { return signalReturn(key, frame); }
    PDAResult operator()(menu::ReniceFrame& frame) const { return reniceMenu(key, frame); }
};
```

### Handler Signature Change (mainMenu example)
```cpp
// Before (Phase 21):
static int mainMenu(const string& key) {
    auto& frame = std::get<menu::MainFrame>(pda.top());
    // ...
    menuMask.set(Menus::Options);
    currentMenu = Menus::Options;
    pda.replace(menu::OptionsFrame{});
    return Switch;
}

// After (Phase 22):
static PDAResult mainMenu(std::string_view key, menu::MainFrame& frame) {
    // frame is passed by reference from the visitor
    // No std::get needed
    auto& y = frame.y;
    auto& selected = frame.selected;
    // ...
    switch (selected) {
        case Options:
            return {menu::PDAAction::Replace, menu::OptionsFrame{}};
        case Help:
            return {menu::PDAAction::Replace, menu::HelpFrame{}};
        case Quit:
            Global::event_queue.push(event::Quit{0});
            Input::interrupt();
            return {menu::PDAAction::NoChange};
    }
    // ...
    if (retval == Changed) {
        // render to Global::overlay
        // ...
        Runner::run("overlay");
    }
    return {menu::PDAAction::NoChange};
}
```

### signalChoose Replace Pattern
```cpp
// After (Phase 22):
static PDAResult signalChoose(std::string_view key, menu::SignalChooseFrame& frame) {
    // ...
    if (is_in(key, "enter", "space") && frame.selected_signal >= 0) {
        signalKillRet = 0;
        if (s_pid < 1) {
            signalKillRet = ESRCH;
        } else if (kill(s_pid, frame.selected_signal) != 0) {
            signalKillRet = errno;
        }
        if (signalKillRet != 0) {
            // Error: replace current frame with SignalReturn error dialog
            return {menu::PDAAction::Replace, menu::SignalReturnFrame{}};
        }
        // Success: close this menu
        return {menu::PDAAction::Pop};
    }
    // ...
}
```

### Rendering and Runner Integration
```cpp
// In the process() loop, after applying PDAAction:
// Rendering is handled by the handler itself (writes to Global::overlay).
// Runner integration:

// After NoChange with content drawn:
if (result.action == menu::PDAAction::NoChange) {
    if (result.needs_redraw) {
        redraw = false;
        Runner::run("all", true, true);
    }
    // The handler already called Runner::run("overlay") if it rendered
    return;
}
```

**Note on Runner::run("overlay") vs Runner::run("all"):** Currently, process() calls `Runner::run("overlay")` for Changed and `Runner::run("all", true, true)` for redraw. In Phase 22, the handler should call `Runner::run("overlay")` after rendering (when it previously returned Changed), and process() calls `Runner::run("all", true, true)` when redraw is consumed.

**Simplification opportunity:** Let process() handle ALL Runner::run calls based on the result, rather than having handlers call it. This means handlers only write to `Global::overlay` and return PDAResult. process() decides whether to call `Runner::run("overlay")` or `Runner::run("all")` based on the result flags. This is cleaner but requires tracking whether the handler rendered content.

**Recommendation:** Keep handlers writing to `Global::overlay` (no change from Phase 21). Have process() call `Runner::run` based on result flags. Add a `bool rendered` field to PDAResult to distinguish "I rendered overlay content" from "nothing changed".

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| menuMask + currentMenu + menuFunc dispatch | std::visit on PDA top frame | Phase 22 (this phase) | Compile-time exhaustive dispatch; no desync between mask/index/vector |
| Switch return code + recursive process() | PDAAction::Replace + non-recursive loop | Phase 22 (this phase) | Eliminates unbounded recursion; all transitions are declarative |
| int return codes (NoChange/Changed/Closed/Switch) | PDAResult struct (PDAAction + payload + flags) | Phase 22 (this phase) | Type-safe, extensible, carries frame payload for Push/Replace |
| menuMask.set(SignalReturn) inside handler | PDAAction::Replace with SignalReturnFrame | Phase 22 (this phase) | No shared mutable state mutation inside handlers |
| File-scope bg string | PDA-owned bg (from Phase 21) | Phase 21, formalized Phase 22 | Clear lifecycle: set on redraw, cleared on Pop/Replace |

**Deprecated/outdated (to be deleted in this phase):**
- `menuMask` (bitset<8>): replaced by PDA stack presence
- `currentMenu` (int): replaced by variant alternative on stack top
- `menuFunc` (vector<ref>): replaced by std::visit with MenuVisitor
- `menuReturnCodes` enum (NoChange/Changed/Closed/Switch): replaced by PDAResult
- `menu_open` / `menu_close_current` / `menu_clear_all` wrappers: no longer needed since menuMask is gone

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 (via FetchContent) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cmake --build build-test --target btop_test && ctest --test-dir build-test -R "MenuPDA\|PDAResult\|PDADispatch" --output-on-failure` |
| Full suite command | `cmake --build build-test --target btop_test && ctest --test-dir build-test --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PDA-04 | PDA dispatch replaces menuMask/currentMenu/menuFunc | integration (compile + grep) | Build btop binary; grep for `menuMask`, `currentMenu`, `menuFunc` in btop_menu.cpp returns zero hits | Wave 0 (grep verification) |
| PDA-04 | std::visit dispatches to correct handler | unit | `ctest --test-dir build-test -R PDAResult --output-on-failure` | Wave 0 |
| PDA-06 | replace() for Main->Options/Help; ESC returns to Normal | unit | `ctest --test-dir build-test -R "Replace\|MainToOptions" --output-on-failure` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build build-test --target btop_test && ctest --test-dir build-test --output-on-failure`
- **Per wave merge:** Full suite (all 307+ tests)
- **Phase gate:** Full suite green + manual verification (open Main->Options, ESC returns to Normal; open Main->Help, ESC returns to Normal; kill signal flow produces SignalReturn dialog on failure)

### Wave 0 Gaps
- [ ] `tests/test_menu_pda.cpp` -- add PDAResult construction and field access tests
- [ ] `tests/test_menu_pda.cpp` -- add PDAAction::Replace preserves stack depth test
- [ ] Grep-based verification to confirm `menuMask`, `currentMenu`, `menuFunc`, `menuReturnCodes` are fully removed from btop_menu.cpp

## Open Questions

1. **Should handlers call Runner::run() or should process() call it?**
   - What we know: Currently, process() calls Runner::run("overlay") for Changed and Runner::run("all") for redraw. Handlers write to Global::overlay.
   - What's unclear: Whether moving ALL Runner::run calls to process() is feasible, given that optionsMenu has special `recollect` and `screen_redraw` paths that call Runner::run("all", false, true).
   - Recommendation: Keep handlers calling Runner::run for special paths (optionsMenu recollect, screen_redraw). Have process() handle the common paths (overlay after normal render, "all" after redraw). Add a `rendered` flag to PDAResult.

2. **Should PDAResult carry a separate "rendered" flag or should NoChange vs Changed be encoded?**
   - What we know: The old int return codes had NoChange (0) and Changed (1) to distinguish "handler did nothing" from "handler rendered new content".
   - What's unclear: Whether PDAResult needs this distinction or if process() can infer it.
   - Recommendation: Add a `bool rendered{false}` field to PDAResult. When true, process() calls `Runner::run("overlay")`. This is cleaner than trying to infer rendering from the action.

3. **How to handle the Menus enum for external callers?**
   - What we know: btop_input.cpp uses `Menu::Menus::Main` etc. in show() calls. The enum maps integers to frame types.
   - What's unclear: Whether Phase 22 should keep the enum as-is or replace it with a different API.
   - Recommendation: Keep the Menus enum in btop_menu.hpp. show() continues to accept int and push the correct frame. Phase 25 cleanup replaces the int-based API with a typed one.

4. **What about Menu::output (extern string)?**
   - What we know: btop_menu.hpp declares `extern string output` but all rendering goes through `Global::overlay`. Searching the codebase, `Menu::output` appears unused.
   - What's unclear: Whether any code still references Menu::output.
   - Recommendation: Verify with grep; if unused, remove in this phase as part of cleanup.

## Sources

### Primary (HIGH confidence)
- `src/btop_menu.cpp` -- complete current source (1928 lines) with all 8 menu functions, process(), show(), legacy dispatch mechanism, single-writer wrappers, PDA integration
- `src/btop_menu.hpp` -- public Menu namespace interface including menuMask, Menus enum, active, mouse_mappings
- `src/btop_menu_pda.hpp` -- MenuFrameVar variant, 8 frame structs, PDAAction enum, MenuPDA class (254 lines)
- `src/btop_input.cpp` -- external caller of Menu::show() (lines 234, 238, 242, 259, 283, 504, 510, 516) and Menu::mouse_mappings reader (line 182)
- `src/btop.cpp` -- main loop Menu::active checks (lines 771, 906, 1009, 1544, 1572)
- `tests/test_menu_pda.cpp` -- 40+ existing tests covering PDA types, operations, frame defaults, invalidate_layout
- `.planning/phases/21-static-local-migration/21-RESEARCH.md` -- Phase 21 patterns and decisions
- `.planning/phases/21-static-local-migration/21-VERIFICATION.md` -- Phase 21 verification confirming all dual-write wrappers work, all functions migrated

### Secondary (MEDIUM confidence)
- C++ standard library documentation for std::visit, std::variant, std::optional -- well-established C++17
- `.planning/REQUIREMENTS.md` -- PDA-04, PDA-06 requirement definitions
- `.planning/ROADMAP.md` -- Phase 22 success criteria and dependency chain

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- using only C++ standard library types already in the codebase (std::visit, std::variant, std::optional)
- Architecture: HIGH -- the dispatch replacement is mechanical (visitor wrapping existing functions); the non-recursive loop is a standard pattern; all code paths traced through the actual source
- Pitfalls: HIGH -- all 7 pitfalls are derived from reading the actual source code line-by-line: every menuMask.set() call, every pda.replace() inside a handler, every recursive process() call, every Runner::pause_output path

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable -- C++ standard library, no external dependencies)
