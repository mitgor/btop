# Phase 21: Static Local Migration - Research

**Researched:** 2026-03-02
**Domain:** C++ function-static local elimination; per-frame state migration in a menu pushdown automaton
**Confidence:** HIGH

## Summary

Phase 21 is the mechanical heart of the menu PDA migration: every menu rendering function must stop using function-static locals and instead read/write members of the frame struct passed by reference. Phase 20 already defined all 8 frame structs with the correct fields and layout/interaction separation. Phase 21 wires them in.

The current code has 8 static menu functions (mainMenu, optionsMenu, helpMenu, sizeError, signalChoose, signalSend, signalReturn, reniceMenu) that use function-static locals for all per-menu state. Each function also uses a shared `bg` string as a first-time-initialization sentinel (`if (bg.empty()) { reset statics; }`). Phase 21 replaces this pattern: each function receives its frame struct by reference, reads/writes struct members instead of statics, and the `bg.empty()` sentinel is eliminated because frame construction IS initialization (all fields have in-class member initializers from Phase 20).

The critical safety mechanism during this migration is a single-writer wrapper (`menu_show`/`menu_close`) that updates both the legacy `menuMask`/`currentMenu` AND the PDA stack atomically, with `assert(menuMask.none() == pda_stack.empty())` as an invariant guard. This dual-write approach means the existing dispatch (menuMask + menuFunc vector) continues to work exactly as before while frames carry the actual state. Phase 22 will then switch dispatch to PDA-based, and Phase 25 will remove the legacy path.

Additionally, this phase implements `invalidate_layout()` -- a method on the frame structs (or a free function) that zeros layout fields (x, y, height) while preserving interaction fields (selected, page, editing state). This is called during resize to force re-computation of geometry while keeping the user's menu state intact.

**Primary recommendation:** Migrate one function at a time in order of complexity (simplest first: sizeError, signalSend, signalReturn, signalChoose, reniceMenu, helpMenu, mainMenu, optionsMenu). For each function, add a frame struct reference parameter, replace `static` locals with frame member access, remove the `bg.empty()` sentinel, and verify the function compiles and behaves identically. Wrap Menu::show/process with single-writer wrappers that keep menuMask and PDA stack in sync.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FRAME-01 | Each frame struct carries per-frame state as members (replacing function-static locals) | Each of the 8 menu functions has catalogued static locals (see Pattern 4 in Phase 20 research) that map 1:1 to frame struct members already defined in btop_menu_pda.hpp. Migration is mechanical: replace `static int x{}` with `frame.x` for each function. |
| FRAME-03 | invalidate_layout() zeros layout fields while preserving interaction fields | Frame structs already separate layout from interaction fields with comment markers. invalidate_layout() is a simple visitor or per-struct method that zeros the layout group. See Architecture Pattern 2 below. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| std::variant | C++17 (C++23 project) | MenuFrameVar discriminated union (from Phase 20) | Already in use; std::visit for invalidate_layout dispatch |
| std::visit | C++17 (C++23 project) | Dispatch over MenuFrameVar for invalidate_layout | Already used in btop_state.hpp and will be used for PDA dispatch |
| std::stack | C++98 (C++23 project) | PDA stack (from Phase 20) | Already in use via MenuPDA class |
| GTest | v1.17.0 | Unit testing for invalidate_layout and frame state migration | Already fetched via FetchContent |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::bitset | C++98 | Legacy menuMask (kept during migration, removed Phase 25) | Dual-write wrapper keeps it in sync with PDA |
| std::cassert | C++11 | Invariant guards in single-writer wrappers | assert(menuMask.none() == pda.empty()) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Per-struct invalidate method | Free function visitor with std::visit | Visitor is cleaner when all 8 types need same pattern; but per-struct method is simpler to implement and maintain since layout fields differ per struct |
| Dual-write (menuMask + PDA) | Direct PDA-only dispatch | Would skip Phase 22 but risks a too-large change; incremental migration is safer |
| Passing frame by mutable reference | Passing frame by pointer | Reference is cleaner, cannot be null, and matches project conventions |

## Architecture Patterns

### Recommended Project Structure
```
src/
  btop_menu_pda.hpp      # MODIFIED: add invalidate_layout() support
  btop_menu.hpp          # UNCHANGED (legacy interface preserved)
  btop_menu.cpp          # MODIFIED: all 8 functions receive frame by ref, single-writer wrappers
tests/
  test_menu_pda.cpp      # MODIFIED: add invalidate_layout tests
```

### Pattern 1: Function Signature Migration (FRAME-01)

**What:** Each menu function gains a frame struct reference parameter. The function reads/writes frame members instead of function-static locals.

**When to use:** Every menu function.

**Before (current code):**
```cpp
static int mainMenu(const string& key) {
    static int y{};
    static int selected{};
    static vector<string> colors_selected;
    static vector<string> colors_normal;
    if (bg.empty()) selected = 0;
    // ... uses y, selected, colors_selected, colors_normal
}
```

**After (Phase 21):**
```cpp
static int mainMenu(const string& key, menu::MainFrame& frame) {
    // No static locals -- all state lives in frame
    // No bg.empty() sentinel -- frame was freshly constructed on push
    auto& y = frame.y;
    auto& selected = frame.selected;
    auto& colors_selected = frame.colors_selected;
    auto& colors_normal = frame.colors_normal;
    // ... body unchanged, reads/writes via frame references
}
```

**Key insight:** Using `auto& y = frame.y;` aliases at the top of the function means the existing body code does not need to change beyond removing `static` and the `bg.empty()` sentinel. This minimizes diff size and review risk.

### Pattern 2: invalidate_layout() (FRAME-03)

**What:** A function or method that zeros layout fields while preserving interaction fields. Called during terminal resize.

**When to use:** On resize, for every frame on the PDA stack (not just the top).

**Implementation options:**

Option A -- Per-struct method (recommended for clarity):
```cpp
// In btop_menu_pda.hpp, add to each frame struct:
struct MainFrame {
    // --- Layout fields ---
    int y{};
    // --- Interaction fields ---
    int selected{};
    std::vector<std::string> colors_selected{};
    std::vector<std::string> colors_normal{};
    // --- Per-frame mouse mappings ---
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};

    void invalidate_layout() {
        y = 0;
        mouse_mappings.clear();
    }
};
```

Option B -- std::visit overload set (apply to the top frame or iterate):
```cpp
struct InvalidateLayout {
    void operator()(menu::MainFrame& f) const { f.y = 0; f.mouse_mappings.clear(); }
    void operator()(menu::OptionsFrame& f) const { f.x = 0; f.y = 0; f.height = 0; f.mouse_mappings.clear(); }
    void operator()(menu::HelpFrame& f) const { f.x = 0; f.y = 0; f.height = 0; f.mouse_mappings.clear(); }
    void operator()(menu::SizeErrorFrame& f) const { f.mouse_mappings.clear(); }
    void operator()(menu::SignalChooseFrame& f) const { f.x = 0; f.y = 0; f.mouse_mappings.clear(); }
    void operator()(menu::SignalSendFrame& f) const { f.mouse_mappings.clear(); }
    void operator()(menu::SignalReturnFrame& f) const { f.mouse_mappings.clear(); }
    void operator()(menu::ReniceFrame& f) const { f.x = 0; f.y = 0; f.mouse_mappings.clear(); }
};
```

**Recommendation:** Option A (per-struct method) is simpler and keeps the invalidation logic co-located with the field definitions. When a layout field is added/removed, the invalidate method is right there. Option B becomes attractive only when the dispatch is needed from outside the struct, but in Phase 21 the function receiving the frame can just call `frame.invalidate_layout()`.

**Critical detail:** `invalidate_layout()` should also clear `mouse_mappings` because those contain absolute terminal coordinates that become stale on resize. It should also set `Menu::redraw = true` so the menu re-renders its visual elements on the next frame.

**What invalidate_layout does NOT do:**
- Does NOT clear interaction fields (selected, page, editing state, nice_edit, etc.)
- Does NOT clear bg (bg is owned by MenuPDA, not individual frames)
- Does NOT clear the PDA stack (menu stays open)

### Pattern 3: Single-Writer Wrapper (menu_show / menu_close)

**What:** Wrapper functions that update both the legacy menuMask/currentMenu AND the PDA stack atomically, maintaining the invariant `menuMask.none() == pda.empty()` during the migration period.

**When to use:** Every point that currently calls `menuMask.set()`, `menuMask.reset()`, `Menu::show()`, or modifies `currentMenu`.

**Example:**
```cpp
// In btop_menu.cpp, file-scope PDA instance:
static menu::MenuPDA pda;

// Wrapper: open a menu (replaces direct menuMask.set())
void menu_show_impl(int menu_enum, menu::MenuFrameVar frame) {
    menuMask.set(menu_enum);
    pda.push(std::move(frame));
    assert(menuMask.none() == pda.empty());
}

// Wrapper: close current menu (replaces direct menuMask.reset())
void menu_close_impl(int menu_enum) {
    menuMask.reset(menu_enum);
    if (!pda.empty()) pda.pop();
    assert(menuMask.none() == pda.empty());
}
```

**Call sites that need wrapping:**
1. `Menu::show()` (line 1875) -- currently sets menuMask and calls process()
2. `Menu::process()` (line 1854) -- menuMask.reset(currentMenu) on Closed return
3. `mainMenu()` (lines 1229-1234) -- sets menuMask for Options/Help and currentMenu
4. `signalChoose()` (lines 1019, 1023) -- sets menuMask for SignalReturn
5. `signalSend()` (line 1139) -- sets menuMask for SignalReturn
6. `process()` (lines 1842-1843) -- resets and sets menuMask for SizeError override

**The assert guard:** `assert(menuMask.none() == pda.empty())` verifies that the legacy system and the new PDA agree on whether any menu is active. This catches any code path that modifies one without the other. It is active only during the migration (Phases 21-24) and removed in Phase 25 along with menuMask.

### Pattern 4: Frame Pass-Through in Dispatch (menuFunc adaptation)

**What:** The existing `menuFunc` dispatch vector calls functions with signature `int(const string& key)`. Phase 21 must adapt this to also pass the frame.

**Approach:** Since the PDA and menuFunc dispatch co-exist during migration, the simplest approach is to have each menu function internally retrieve its frame from the file-scope PDA instance:

```cpp
static int mainMenu(const string& key) {
    auto& frame = std::get<menu::MainFrame>(pda.top());
    // ... rest of function uses frame.y, frame.selected, etc.
}
```

**Why this is better than changing the menuFunc signature:** The menuFunc vector is a `vector<ref(static function)>` with a fixed signature. Changing every function's signature would require changing the vector type and the dispatch call in `process()`. Since Phase 22 will replace menuFunc entirely with std::visit, it's not worth changing the interface now. Instead, each function reaches into the file-scope PDA to get its frame.

**Prerequisite:** The file-scope `pda` instance must be populated before `process()` dispatches. The `menu_show_impl` wrapper handles this.

### Pattern 5: bg String Migration

**What:** The current code uses the file-scope `string bg` as both a rendering cache AND a first-time initialization sentinel. Phase 21 moves bg ownership to the PDA.

**Current pattern (anti-pattern):**
```cpp
string bg;  // file-scope

static int mainMenu(const string& key) {
    if (bg.empty()) selected = 0;  // sentinel: reset on first call
    if (redraw) {
        bg = Draw::banner_gen(y, 0, true);  // compute bg
        // ... render using bg
    }
}
```

**Phase 21 pattern:**
```cpp
static menu::MenuPDA pda;

static int mainMenu(const string& key) {
    auto& frame = std::get<menu::MainFrame>(pda.top());
    // No bg.empty() sentinel -- frame.selected was initialized by constructor
    if (redraw) {
        pda.set_bg(Draw::banner_gen(frame.y, 0, true));
        // ... render using pda.bg()
    }
    auto& out = Global::overlay;
    out = pda.bg() + Fx::reset + Fx::b;
    // ...
}
```

**Key points:**
- `pda.set_bg()` replaces `bg = ...` assignments
- `pda.bg()` replaces `bg` reads
- `pda.clear_bg()` replaces `bg.clear()` in process() cleanup
- The `bg.empty()` sentinel check is deleted entirely; frame constructor handles initialization

### Anti-Patterns to Avoid

- **Changing menuFunc vector signature in Phase 21:** The vector type is `vector<ref(function)>` with a fixed `int(const string&)` signature. Do not try to add a frame parameter to this interface. Instead, have each function retrieve its frame from the file-scope PDA. Phase 22 eliminates menuFunc entirely.

- **Clearing interaction fields in invalidate_layout():** On resize, only layout fields (x, y, height) and mouse_mappings should be zeroed. If you zero `selected`, `page`, `editing`, etc., the user loses their menu state when the terminal is resized.

- **Removing the bg.empty() sentinel without ensuring frame initialization:** The `bg.empty()` check currently resets statics like `selected = 0` on first open. After migration, this reset is unnecessary because frame construction provides the correct initial values. But verify that every field used in the sentinel reset has a matching in-class member initializer in the frame struct.

- **Modifying the PDA stack inside a function that was dispatched by menuFunc:** During Phase 21, menuFunc dispatches to static functions. Those functions must NOT push/pop/replace the PDA directly. They can signal intent via the existing return codes (Closed, Switch). The caller (process()) handles PDA operations.

- **Forgetting to wrap internal menuMask.set() calls:** `signalChoose()` and `signalSend()` directly set `menuMask.set(SignalReturn)` inside the handler. These must go through the wrapper or at minimum also push a SignalReturnFrame onto the PDA.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Layout field zeroing | Manual per-field zeroing scattered across code | invalidate_layout() method on each frame struct | Keeps zeroing logic co-located with field definitions; impossible to forget a field |
| Dual-write synchronization | Ad-hoc menuMask + PDA updates at each call site | Centralized menu_show_impl/menu_close_impl wrappers | Single point of truth; assert guard catches desync |
| Frame retrieval from PDA | Dynamic type checking at every frame access | std::get<FrameType>(pda.top()) with known type | Each function knows its own frame type; compile-time type safety |

**Key insight:** The migration is mechanical -- every static local maps to a frame member, every bg.empty() sentinel is deleted, every menuMask.set() is wrapped. The discipline is in NOT being clever: keep the existing function bodies as unchanged as possible, just redirect reads/writes to frame members.

## Common Pitfalls

### Pitfall 1: PDA Empty on First Dispatch
**What goes wrong:** `process()` dispatches to `mainMenu()`, which calls `std::get<MainFrame>(pda.top())`, but the PDA is empty because `show()` didn't push a frame.
**Why it happens:** `Menu::show()` currently only sets `menuMask.set(menu)`. If the wrapper forgets to push a frame, pda.top() will assert-fail.
**How to avoid:** `menu_show_impl()` must ALWAYS push a frame when setting menuMask. The assert guard `assert(menuMask.none() == pda.empty())` catches this.
**Warning signs:** Debug-mode assert failures on `pda.top()` immediately after menu open.

### Pitfall 2: Switch Return Code and menuMask Internal Mutation
**What goes wrong:** `mainMenu()` returns `Switch` after setting `menuMask.set(Menus::Options)` and `currentMenu = Menus::Options`. The process() handler then clears bg and calls itself recursively. If the PDA isn't updated in tandem, the invariant breaks.
**Why it happens:** The `Switch` code path is the most complex: it involves clearing the current menu's bg, setting up a new menu, and re-entering process(). All three mutations (menuMask set, currentMenu set, bg clear) need PDA equivalents.
**How to avoid:** Replace the internal `menuMask.set(Menus::Options)` in mainMenu() with a call to the wrapper that also does `pda.replace(menu::OptionsFrame{})`. Since Main->Options is a replace (not push), the PDA depth stays 1.
**Warning signs:** Assert failures after navigating Main->Options or Main->Help.

### Pitfall 3: signalChoose/signalSend Internal menuMask.set(SignalReturn)
**What goes wrong:** `signalChoose()` (lines 1019, 1023) and `signalSend()` (line 1139) directly set `menuMask.set(SignalReturn)` before returning `Closed`. This is a "push after pop" pattern: the current menu closes but immediately opens SignalReturn.
**Why it happens:** The kill() call returns an error, so the function pushes a SignalReturn error dialog.
**How to avoid:** These internal `menuMask.set()` calls must also push a `SignalReturnFrame` onto the PDA. But since the function also returns `Closed` (which triggers a pop in process()), the sequencing must be: (1) push SignalReturnFrame, (2) return Closed, (3) process() pops the current frame -- but wait, that would pop the just-pushed SignalReturn frame. **Solution:** The push of SignalReturn should happen in process() when it detects that signalKillRet is set, NOT inside the handler. Or, the return code must be adapted (e.g., a new code or deferred push). The cleanest approach: keep the existing menuMask.set(SignalReturn) call as-is during Phase 21 (it sets the bit for next dispatch), and have process() also push a SignalReturnFrame when it detects the bit was set.
**Warning signs:** SignalReturn dialog not appearing after failed kill(), or PDA stack depth mismatch.

### Pitfall 4: SizeError Override Clears menuMask But Not PDA
**What goes wrong:** In process() (lines 1842-1843), when the terminal is too small, `menuMask.reset()` then `menuMask.set(SizeError)` -- this clears ALL menus and only opens SizeError. The PDA must be similarly cleared and a SizeErrorFrame pushed.
**Why it happens:** SizeError is an override that cancels whatever menu was being opened.
**How to avoid:** The SizeError path in process() must also call a helper that empties the PDA stack and pushes a SizeErrorFrame.
**Warning signs:** Assert failure on menuMask.none() == pda.empty() after SizeError override.

### Pitfall 5: msgBox Ownership During Migration
**What goes wrong:** `sizeError`, `signalSend`, and `signalReturn` use the shared `msgBox messageBox` variable. In Phase 20, their frame structs are minimal (only mouse_mappings). If Phase 21 tries to move messageBox into the frame, it requires significant header changes and messageBox is also used by optionsMenu's warning dialog.
**Why it happens:** messageBox is a shared mutable global -- multiple functions write to it.
**How to avoid:** In Phase 21, keep `messageBox` as a file-scope variable (shared). Do NOT try to move it into frame structs. The minimal frames (SizeErrorFrame, SignalSendFrame, SignalReturnFrame) continue to have only mouse_mappings. Moving messageBox ownership is a Phase 22+ concern or may remain shared.
**Warning signs:** Trying to add msgBox as a member of SizeErrorFrame/SignalSendFrame/SignalReturnFrame in Phase 21.

### Pitfall 6: optionsMenu Static const optionsList
**What goes wrong:** optionsMenu has a `static const` unordered_map `optionsList` (line 1288). This is NOT a per-frame field -- it's a compile-time constant lookup table. If you try to move it into the frame struct, it wastes memory and complicates construction.
**Why it happens:** Confusing `static const` (optimization for immutable data) with `static` (mutable per-call state).
**How to avoid:** Leave `static const` variables in place. Only migrate mutable `static` locals to frame members. The `static const optionsList` stays as-is.
**Warning signs:** Adding large const maps to OptionsFrame.

### Pitfall 7: Thread Safety of File-Scope PDA Instance
**What goes wrong:** If the PDA is accessed from multiple threads, it could race. Currently, all menu operations happen on the main thread, but `Menu::active` is an `atomic<bool>` because the runner thread reads it.
**Why it happens:** The runner thread checks `Menu::active` to decide whether to skip output. It does NOT access menu internals.
**How to avoid:** The file-scope PDA instance is main-thread-only (same as the current menuMask, currentMenu, and all static locals). `Menu::active` remains atomic for cross-thread signaling. No mutex is needed on the PDA.
**Warning signs:** TSan reports on PDA access (would indicate an unexpected thread accessing it).

## Code Examples

### Complete Migration of signalReturn (Simplest Case)

**Before:**
```cpp
static int signalReturn(const string& key) {
    if (redraw) {
        vector<string> cont_vec;
        // ... build cont_vec based on signalKillRet
        messageBox = Menu::msgBox{50, 0, cont_vec, "error"};
        Global::overlay = messageBox();
    }
    auto ret = messageBox.input(key);
    if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
        messageBox.clear();
        return Closed;
    }
    else if (redraw) return Changed;
    return NoChange;
}
```

**After:**
```cpp
static int signalReturn(const string& key) {
    // No static locals to migrate -- signalReturn has none
    // No frame access needed -- it only uses the shared messageBox
    // auto& frame = std::get<menu::SignalReturnFrame>(pda.top()); // available but unused

    if (redraw) {
        vector<string> cont_vec;
        // ... build cont_vec based on signalKillRet
        messageBox = Menu::msgBox{50, 0, cont_vec, "error"};
        Global::overlay = messageBox();
    }
    auto ret = messageBox.input(key);
    if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
        messageBox.clear();
        return Closed;
    }
    else if (redraw) return Changed;
    return NoChange;
}
```

Note: signalReturn, signalSend, and sizeError have NO static locals (they only use the shared messageBox). Their migration is trivial -- just ensure the PDA has the correct frame type pushed.

### Migration of helpMenu (Medium Complexity)

**Before:**
```cpp
static int helpMenu(const string& key) {
    static int y{};
    static int x{};
    static int height{};
    static int page{};
    static int pages{};

    if (bg.empty()) page = 0;
    int retval = Changed;
    if (redraw) {
        y = max(1, Term::height/2 - 4 - (int)(help_text.size() / 2));
        x = Term::width/2 - 39;
        // ...
        bg = Draw::banner_gen(y, 0, true);
        bg += Draw::createBox(x, y + 6, 78, height, ...);
    }
    // ... uses y, x, height, page, pages
    if (retval == Changed) {
        auto& out = Global::overlay;
        out = bg;
        // ...
    }
    return (redraw ? Changed : retval);
}
```

**After:**
```cpp
static int helpMenu(const string& key) {
    auto& frame = std::get<menu::HelpFrame>(pda.top());
    // Alias frame members for minimal body changes
    auto& y = frame.y;
    auto& x = frame.x;
    auto& height = frame.height;
    auto& page = frame.page;
    auto& pages = frame.pages;

    // bg.empty() sentinel REMOVED -- frame constructor initialized page=0
    int retval = Changed;
    if (redraw) {
        y = max(1, Term::height/2 - 4 - (int)(help_text.size() / 2));
        x = Term::width/2 - 39;
        // ...
        pda.set_bg(Draw::banner_gen(y, 0, true));
        auto bg_str = pda.bg();  // local copy if needed for concatenation
        // Or: modify pda bg in place
    }
    // ... rest uses y, x, height, page, pages (now aliases to frame)
    if (retval == Changed) {
        auto& out = Global::overlay;
        out = pda.bg();
        // ...
    }
    return (redraw ? Changed : retval);
}
```

### invalidate_layout() Per-Struct Method

```cpp
// In btop_menu_pda.hpp

struct MainFrame {
    // --- Layout fields ---
    int y{};
    // --- Interaction fields ---
    int selected{};
    std::vector<std::string> colors_selected{};
    std::vector<std::string> colors_normal{};
    // --- Per-frame mouse mappings ---
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};

    void invalidate_layout() {
        y = 0;
        mouse_mappings.clear();
    }
};

struct OptionsFrame {
    // ... fields ...
    void invalidate_layout() {
        x = 0;
        y = 0;
        height = 0;
        mouse_mappings.clear();
    }
};

// ... similar for all 8 frame structs

// In MenuPDA class:
void invalidate_all_layouts() {
    // Must iterate the entire stack, not just top
    // std::stack doesn't expose iteration, so use the underlying container
    // Option: temporarily pop/push, or switch to a deque/vector accessor
}
```

**Important complication:** `std::stack` does not expose its underlying container for iteration. To invalidate ALL frames (not just the top), either:
1. Add a `for_each` method to MenuPDA that accesses the protected `c` member (via inheritance trick or friend)
2. Only invalidate the top frame (since lower frames will be re-rendered when they become top anyway)
3. Use a different approach: make the `redraw` flag sufficient (when redraw=true, the function recomputes layout from scratch)

**Recommendation:** Option 2 or 3. The current resize behavior sets `Menu::redraw = true` (btop_draw.cpp line 2768), which causes the active menu function to recompute all layout fields on the next call. Lower frames on the stack will get `redraw=true` when they become the top frame (after a pop). Therefore, `invalidate_layout()` only needs to zero the TOP frame's layout fields + clear mouse_mappings. The `redraw = true` flag handles the actual recomputation.

### MenuPDA invalidate_layout for Top Frame

```cpp
// In MenuPDA class (btop_menu_pda.hpp):
void invalidate_layout() {
    if (!stack_.empty()) {
        std::visit([](auto& frame) { frame.invalidate_layout(); }, stack_.top());
    }
}
```

### Single-Writer Wrappers

```cpp
// In btop_menu.cpp, before process():

static menu::MenuPDA pda;

// Push a new menu frame, keeping menuMask in sync
static void menu_open(int menu_enum, menu::MenuFrameVar frame) {
    menuMask.set(menu_enum);
    pda.push(std::move(frame));
    assert(menuMask.none() == pda.empty());
}

// Close the current menu, keeping menuMask in sync
static void menu_close_current(int menu_enum) {
    menuMask.reset(menu_enum);
    if (!pda.empty()) pda.pop();
    // Note: don't assert here because process() may close multiple
    // menus in sequence (the recursive call pattern)
}

// Clear all menus (for SizeError override)
static void menu_clear_all() {
    menuMask.reset();
    while (!pda.empty()) pda.pop();
    assert(menuMask.none() == pda.empty());
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Function-static locals in each menu function | Per-frame struct members passed by reference | Phase 21 (this phase) | Testable, restartable, no hidden persistent state |
| bg.empty() as first-time initialization sentinel | In-class member initializers (NSDMI) from Phase 20 | Phase 21 (this phase) | Eliminates an entire category of bugs; frame construction IS initialization |
| Direct menuMask.set()/reset() calls scattered in code | Single-writer wrappers (menu_open/menu_close) | Phase 21 (this phase) | Atomic dual-write ensures menuMask and PDA stay in sync |
| No layout invalidation on resize (just redraw=true) | invalidate_layout() zeros geometry fields | Phase 21 (this phase) | Explicit contract: resize preserves interaction, invalidates layout |

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 (via FetchContent) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cmake --build build-test --target btop_test && ctest --test-dir build-test -R "MenuPDA\|Frame\|InvalidateLayout" --output-on-failure` |
| Full suite command | `cmake --build build-test --target btop_test && ctest --test-dir build-test --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FRAME-01 | Frame struct members replace function-static locals | integration (compile + run btop) | Build btop binary and verify no static locals remain in menu functions | Wave 0 (grep-based verification) |
| FRAME-03 | invalidate_layout() zeros layout, preserves interaction | unit | `ctest --test-dir build-test -R InvalidateLayout --output-on-failure` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build build-test --target btop_test && ctest --test-dir build-test --output-on-failure`
- **Per wave merge:** Full suite (all 297+ tests)
- **Phase gate:** Full suite green + manual verification (open every menu, navigate, resize terminal, verify correct behavior)

### Wave 0 Gaps
- [ ] `tests/test_menu_pda.cpp` -- add invalidate_layout() tests for each frame type (FRAME-03)
- [ ] Grep-based verification script to confirm no `static int/bool/string/vector/bitset` locals remain in menu function bodies (FRAME-01, CLEAN-04 precursor)

## Open Questions

1. **Should invalidate_layout() iterate the entire PDA stack or just the top?**
   - What we know: std::stack doesn't expose iteration. The current resize sets `Menu::redraw = true`, which causes the active function to recompute layout. Lower frames will get redraw when they become top (after pop).
   - What's unclear: Whether there's a scenario where a lower frame's stale layout coordinates cause a bug before it becomes top.
   - Recommendation: Only invalidate the top frame. The `redraw = true` flag ensures re-layout when any frame becomes active. If a deeper invalidation is needed later, the stack can be replaced with a vector wrapper.

2. **How to handle bg string concatenation in optionsMenu?**
   - What we know: optionsMenu builds bg incrementally: `bg = Draw::banner_gen(...) + Draw::createBox(...) + Mv::to(...) + ...`. If bg lives in the PDA, each concatenation requires `pda.set_bg(pda.bg() + ...)`.
   - What's unclear: Whether this creates excessive string copies.
   - Recommendation: Build bg in a local string, then call `pda.set_bg(std::move(local_bg))` once. This matches the current pattern where bg is built entirely during the `if (redraw)` block.

3. **Should the file-scope PDA instance be exposed in the header?**
   - What we know: Phase 21 needs the PDA accessible to all 8 menu functions and to process/show. All are in the same translation unit (btop_menu.cpp).
   - What's unclear: Whether Phase 22/23 will need PDA access from other files (btop_draw.cpp sets Menu::redraw, btop_input.cpp calls Menu::show).
   - Recommendation: Keep it file-scope (`static menu::MenuPDA pda;`) in Phase 21. Phase 22 may move it to a namespace-scope variable in btop_menu.hpp if needed for external access, but during migration the less exposure the better.

4. **How to handle the Switch return code's interaction with PDA?**
   - What we know: mainMenu returns Switch after setting menuMask.set(Menus::Options/Help). process() then clears bg, sets redraw=true, and calls process() recursively. The Switch path needs to replace the top PDA frame (Main -> Options or Main -> Help).
   - What's unclear: Whether the replace should happen inside mainMenu (violating the "no PDA mutation inside handler" rule) or in process() when it handles Switch.
   - Recommendation: Have process() handle the replace. When it gets Switch, it should check what menuMask bit was set and replace the PDA top accordingly. This keeps PDA mutations in the single-writer layer.

## Sources

### Primary (HIGH confidence)
- `src/btop_menu.cpp` -- complete source for all 8 menu functions, process(), show(), static locals inventory
- `src/btop_menu.hpp` -- Menu namespace interface (menuMask, Menus enum, active bool, mouse_mappings, output)
- `src/btop_menu_pda.hpp` -- Phase 20 output: MenuFrameVar, frame structs, PDAAction, MenuPDA class
- `src/btop_draw.cpp:2768` -- resize path sets Menu::redraw = true when menu active
- `src/btop.cpp:1572` -- main loop dispatches to Menu::process(key) when Menu::active
- `src/btop_input.cpp:182` -- mouse routing checks Menu::active for mouse_mappings source
- `tests/test_menu_pda.cpp` -- 31 existing tests for PDA types from Phase 20
- `.planning/phases/20-pda-types-skeleton/20-RESEARCH.md` -- Pattern 4 (static local inventory)

### Secondary (MEDIUM confidence)
- C++ standard library documentation for std::visit, std::get, std::stack -- well-established C++17
- `.planning/REQUIREMENTS.md` -- FRAME-01, FRAME-03 requirement definitions
- `.planning/ROADMAP.md` -- Phase 21 success criteria and dependency chain

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- using only C++ standard library types already in use in this codebase
- Architecture: HIGH -- the migration pattern is mechanical (static local -> frame member), the bg sentinel elimination follows from Phase 20's NSDMI guarantee, and the dual-write wrapper is a standard migration safety net
- Pitfalls: HIGH -- all 7 pitfalls are derived from reading the actual source code and tracing every menuMask.set() call site, every bg.empty() check, and every static local declaration

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable -- C++ standard library, no external dependencies)
