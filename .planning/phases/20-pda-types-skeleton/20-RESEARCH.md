# Phase 20: PDA Types + Skeleton - Research

**Researched:** 2026-03-02
**Domain:** C++23 pushdown automaton data structures (std::variant, std::stack, typed frame structs)
**Confidence:** HIGH

## Summary

Phase 20 defines all typed data structures for a menu pushdown automaton (PDA) that will eventually replace the current `menuMask` bitset + `currentMenu` int + `menuFunc` dispatch vector in btop's menu system. The phase is strictly about data definitions and isolated testability -- no behavior change to the running application.

The current menu system uses `bitset<8> menuMask` to track which menus are "stacked," `int currentMenu` to track which is active, a `vector<ref>` of 8 static functions for dispatch, function-static locals for all per-menu state, a shared `string bg` for the background capture, and `atomic<bool> Menu::active` as the cross-thread visibility flag. The PDA will model this as a typed stack of frame structs (each carrying its own layout and interaction state), with `std::visit` dispatch on the top frame.

This project already has a well-established pattern for variant-based state machines from v1.1 (`btop_state.hpp`: `AppStateVar` variant with `state::Running`, `state::Quitting`, etc.) and comprehensive GTest infrastructure (266+ tests). Phase 20 follows the exact same pattern -- define structs, define variant, define operations, write tests.

**Primary recommendation:** Follow the established `btop_state.hpp` pattern exactly: per-frame structs in a `menu` namespace, a `MenuFrameVar` variant, a `PDAAction` enum, and a `MenuPDA` class wrapping `std::stack<MenuFrameVar, std::vector<MenuFrameVar>>`. All code lives in a new `src/btop_menu_pda.hpp` header, testable in isolation with no link dependencies on the rest of btop.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PDA-01 | MenuFrameVar std::variant defined with alternatives for all 8 menus | Pattern: follow AppStateVar from btop_state.hpp; 8 frame structs as variant alternatives |
| PDA-02 | MenuPDA class provides push/pop/replace/top/empty on std::stack<MenuFrameVar, std::vector<MenuFrameVar>> | Standard C++23 std::stack with vector backing; see Architecture Pattern 1 |
| PDA-03 | Frame handlers return PDAAction enum (Push/Pop/Replace/NoChange) instead of modifying stack during std::visit | Pattern: return-value protocol (see Architecture Pattern 2); prevents variant self-modification UB |
| PDA-05 | bg string lifecycle tied to stack depth (captured on first push, cleared on final pop) | MenuPDA owns bg as private member; push() captures if stack was empty, pop() clears if stack becomes empty |
| FRAME-02 | Frame structs split into layout fields (x, y, width, height) and interaction fields (selected, page, entered text) | See Architecture Pattern 3; each frame struct has clearly separated field groups |
| FRAME-04 | Frame constructors unconditionally initialize all fields | All fields get in-class member initializers (C++11 NSDMI); no bg.empty() sentinel pattern |
| FRAME-05 | Per-frame mouse_mappings owned by frame struct (only top frame's mappings active) | Each frame struct has `std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings` as value member |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| std::variant | C++17 (C++23 project) | Type-safe discriminated union for frame alternatives | Already used for AppStateVar; zero dependencies |
| std::stack | C++98 (C++23 project) | LIFO container adaptor for PDA stack | Standard container, exactly models pushdown semantics |
| std::vector | C++98 (C++23 project) | Backing container for std::stack (instead of default std::deque) | Contiguous memory, no deque chunk overhead; max depth ~4 |
| std::visit | C++17 (C++23 project) | Pattern-matching dispatch on variant | Already used in btop_state.hpp to_tag() and btop.cpp dispatch_event() |
| GTest | v1.17.0 | Unit testing | Already fetched via FetchContent in tests/CMakeLists.txt |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::unordered_map | C++11 | Per-frame mouse_mappings | Each frame struct owns its own mouse_mappings |
| std::string | C++98 | bg string, text fields | Per-frame string fields (bg is owned by MenuPDA) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| std::stack | Raw std::vector with push_back/pop_back | std::stack provides cleaner semantics, no random access (which we don't want) |
| std::variant | Inheritance + virtual dispatch | Would work but inconsistent with v1.1 architecture; variant gives compile-time exhaustiveness |
| External FSM library (Boost.SML, TinyFSM) | N/A | Out of scope per REQUIREMENTS.md: "Violates no-new-dependencies constraint" |

## Architecture Patterns

### Recommended Project Structure
```
src/
  btop_menu_pda.hpp      # NEW: MenuFrameVar, frame structs, PDAAction, MenuPDA class
  btop_menu.hpp          # EXISTING: unchanged in this phase
  btop_menu.cpp          # EXISTING: unchanged in this phase
  btop_state.hpp         # EXISTING: pattern reference (AppStateVar)

tests/
  test_menu_pda.cpp      # NEW: unit tests for PDA types and operations
  CMakeLists.txt          # MODIFIED: add test_menu_pda.cpp to btop_test sources
```

### Pattern 1: PDA Stack Class (MenuPDA)
**What:** A class wrapping `std::stack<MenuFrameVar, std::vector<MenuFrameVar>>` with push/pop/replace/top/empty operations and bg string lifecycle management.
**When to use:** This is the central data structure for the menu system.
**Example:**
```cpp
// src/btop_menu_pda.hpp
#pragma once

#include <stack>
#include <string>
#include <variant>
#include <vector>
#include <cassert>

namespace menu {

// Forward: frame structs defined below
struct MainFrame;
struct OptionsFrame;
struct HelpFrame;
struct SizeErrorFrame;
struct SignalChooseFrame;
struct SignalSendFrame;
struct SignalReturnFrame;
struct ReniceFrame;

/// PDA-01: Discriminated union of all menu frames.
using MenuFrameVar = std::variant<
    MainFrame,
    OptionsFrame,
    HelpFrame,
    SizeErrorFrame,
    SignalChooseFrame,
    SignalSendFrame,
    SignalReturnFrame,
    ReniceFrame
>;

/// PDA-03: Actions returned by frame handlers (no stack mutation inside visit).
enum class PDAAction {
    NoChange,   // Frame handled input, no stack change
    Push,       // Push a new frame onto the stack
    Pop,        // Pop the current frame
    Replace,    // Replace the top frame (Main->Options, Main->Help)
};

/// PDA-02: Pushdown automaton stack with bg lifecycle.
class MenuPDA {
    std::stack<MenuFrameVar, std::vector<MenuFrameVar>> stack_;
    std::string bg_;  // PDA-05: captured on first push, cleared on final pop

public:
    /// Push a frame. If stack was empty, caller should capture bg.
    void push(MenuFrameVar frame) {
        stack_.push(std::move(frame));
    }

    /// Pop the top frame. Caller should clear bg if stack becomes empty.
    void pop() {
        assert(!stack_.empty());
        stack_.pop();
    }

    /// Replace the top frame (for Main->Options/Help transitions).
    void replace(MenuFrameVar frame) {
        assert(!stack_.empty());
        stack_.pop();
        stack_.push(std::move(frame));
    }

    /// Access the top frame.
    [[nodiscard]] MenuFrameVar& top() {
        assert(!stack_.empty());
        return stack_.top();
    }
    [[nodiscard]] const MenuFrameVar& top() const {
        assert(!stack_.empty());
        return stack_.top();
    }

    [[nodiscard]] bool empty() const noexcept { return stack_.empty(); }
    [[nodiscard]] std::size_t depth() const noexcept { return stack_.size(); }

    /// PDA-05: bg lifecycle
    [[nodiscard]] const std::string& bg() const noexcept { return bg_; }
    void set_bg(std::string bg) { bg_ = std::move(bg); }
    void clear_bg() { bg_.clear(); bg_.shrink_to_fit(); }
};

} // namespace menu
```

### Pattern 2: Return-Value Protocol (PDAAction)
**What:** Frame handlers return a PDAAction enum value instead of mutating the stack directly. The caller (future dispatch loop) applies the action after std::visit completes.
**When to use:** Always -- this is a hard requirement (PDA-03) driven by the UB risk of modifying a variant while visiting it.
**Why critical:** In the current code, `mainMenu()` directly sets `menuMask.set(Menus::Options)` and `currentMenu = Menus::Options` mid-handler. If this pattern were ported naively into `std::visit`, it would modify the variant being visited, causing undefined behavior. The return-value pattern eliminates this entirely.
**Example:**
```cpp
// Future dispatch pattern (Phase 22 will implement this, Phase 20 just defines the types):
// auto action = std::visit([&](auto& frame) { return frame.handle_input(key); }, pda.top());
// switch (action) {
//     case PDAAction::Push:    pda.push(next_frame); break;
//     case PDAAction::Pop:     pda.pop(); break;
//     case PDAAction::Replace: pda.replace(next_frame); break;
//     case PDAAction::NoChange: break;
// }
```

### Pattern 3: Layout/Interaction Struct Split (FRAME-02)
**What:** Each frame struct separates layout fields (position, size, bg) from interaction fields (selection state, page, entered text). All fields have in-class member initializers.
**When to use:** Every frame struct.
**Why critical:** On terminal resize, layout fields must be invalidated (zeroed) while interaction fields are preserved. If they're mixed together, invalidation either loses user state or requires field-by-field logic. The split makes `invalidate_layout()` a trivial operation (Phase 21).
**Example:**
```cpp
struct OptionsFrame {
    // --- Layout fields (invalidated on resize) ---
    int x{};
    int y{};
    int height{};

    // --- Interaction fields (preserved across resize) ---
    int page{};
    int pages{};
    int selected{};
    int select_max{};
    int item_height{};
    int selected_cat{};
    int max_items{};
    int last_sel{};
    bool editing{false};
    Draw::TextEdit editor{};
    std::string warnings{};
    std::bitset<8> selPred{};

    // --- Per-frame mouse mappings (FRAME-05) ---
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};
```

### Pattern 4: Frame Struct Inventory (from current static locals)
**What:** Each of the 8 current menu functions has specific static local variables that must become frame struct members.

| Menu Function | Current Static Locals | Layout Fields | Interaction Fields |
|--------------|----------------------|---------------|-------------------|
| `mainMenu` | y, selected, colors_selected, colors_normal | y | selected, colors_selected, colors_normal |
| `optionsMenu` | y, x, height, page, pages, selected, select_max, item_height, selected_cat, max_items, last_sel, editing, editor, warnings, selPred | x, y, height | page, pages, selected, select_max, item_height, selected_cat, max_items, last_sel, editing, editor, warnings, selPred |
| `helpMenu` | y, x, height, page, pages | x, y, height | page, pages |
| `signalChoose` | x, y, selected_signal | x, y | selected_signal |
| `signalSend` | (none beyond shared messageBox) | (none) | (uses shared messageBox) |
| `signalReturn` | (none beyond shared messageBox) | (none) | (uses shared messageBox) |
| `sizeError` | (none beyond shared messageBox) | (none) | (uses shared messageBox) |
| `reniceMenu` | x, y, selected_nice, nice_edit | x, y | selected_nice, nice_edit |

**Note:** `signalSend`, `signalReturn`, and `sizeError` use the shared `msgBox messageBox`. In Phase 20 we define empty or minimal frames for these; Phase 21 will handle the actual migration of msgBox ownership.

### Anti-Patterns to Avoid
- **Modifying the variant inside std::visit:** This is undefined behavior. The visitor must return a PDAAction; the caller applies it after visit returns. (Pitfall 1)
- **Using the bg.empty() sentinel for initialization:** The current code uses `if (bg.empty())` to detect "first time" and initialize statics. Frame constructors must unconditionally initialize all fields. The bg string is owned by MenuPDA, not individual frames. (Pitfall 3)
- **Putting rendering logic in frame structs:** Phase 20 is types-only. Frame structs carry data; rendering stays in the existing functions until Phase 21.
- **Including btop_menu.hpp in btop_menu_pda.hpp:** The new header should have minimal dependencies. It needs `Input::Mouse_loc` (from btop_input.hpp) and `Draw::TextEdit` (from btop_draw.hpp). It should NOT include btop_menu.hpp to avoid circular dependencies.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Stack data structure | Custom linked list or vector wrapper | `std::stack<MenuFrameVar, std::vector<MenuFrameVar>>` | Standard, well-tested, exact semantics needed |
| Discriminated union | Tagged union with manual dispatch | `std::variant` + `std::visit` | Type safety, compile-time exhaustiveness checking |
| Frame-type identification | Runtime type tags or enums | `std::holds_alternative<T>()` and `std::get<T>()` | Already the project pattern from v1.1 |

**Key insight:** The entire PDA infrastructure uses only C++ standard library types. No external libraries are needed. This is deliberate -- the project has a hard no-new-dependencies constraint.

## Common Pitfalls

### Pitfall 1: Variant Self-Modification During Visit (UB)
**What goes wrong:** If a frame handler modifies the PDA stack (push/pop/replace) while `std::visit` is executing on the variant, this is undefined behavior.
**Why it happens:** The current code's `mainMenu()` directly mutates `menuMask` and `currentMenu` inside the handler. Naively porting this to `std::visit` on a variant-based stack would be UB.
**How to avoid:** Frame handlers return `PDAAction` enum values. The dispatch loop applies the action AFTER `std::visit` completes. Phase 20 defines the types; Phase 22 implements the dispatch.
**Warning signs:** Any code that calls `pda.push()`, `pda.pop()`, or `pda.replace()` inside a `std::visit` lambda.

### Pitfall 2: Header Dependency Cycles
**What goes wrong:** `btop_menu_pda.hpp` includes `btop_menu.hpp` which includes `btop_input.hpp` which needs types from `btop_menu_pda.hpp`.
**Why it happens:** Frame structs need `Input::Mouse_loc` and `Draw::TextEdit`, creating potential circular dependencies.
**How to avoid:** `btop_menu_pda.hpp` includes only `btop_input.hpp` (for `Mouse_loc`) and `btop_draw.hpp` (for `Draw::TextEdit`). It does NOT include `btop_menu.hpp`. The existing headers don't include `btop_menu_pda.hpp` in Phase 20 -- that connection is made in Phase 21/22.
**Warning signs:** Compilation errors about incomplete types or "already included" warnings.

### Pitfall 3: bg.empty() Reset Sentinel Leaking Into New Code
**What goes wrong:** Frame constructors use `bg.empty()` to detect first-time initialization, replicating the current anti-pattern.
**Why it happens:** Developers copy existing patterns when migrating.
**How to avoid:** All frame struct fields have in-class member initializers. The bg string lives in MenuPDA, not in individual frames. Frame construction IS initialization -- no sentinel checks needed.
**Warning signs:** Any `if (bg.empty())` or `if (x == 0)` guards in frame constructors.

### Pitfall 4: std::deque as Default Stack Backing
**What goes wrong:** `std::stack` defaults to `std::deque`, which allocates in chunks. For a stack of max depth ~4, this wastes memory.
**Why it happens:** Using `std::stack<MenuFrameVar>` without specifying the backing container.
**How to avoid:** Explicitly specify: `std::stack<MenuFrameVar, std::vector<MenuFrameVar>>`. Vector is optimal for small, known-max-depth stacks.
**Warning signs:** `std::stack<MenuFrameVar>` without second template argument.

### Pitfall 5: Making Frame Structs Too Smart in Phase 20
**What goes wrong:** Adding handle_input(), render(), or other behavior methods to frame structs during Phase 20, then discovering the interface doesn't work when wiring dispatch in Phase 22.
**Why it happens:** Eagerness to "finish" the design.
**How to avoid:** Phase 20 is types-only. Frame structs are plain data. No methods beyond constructors. Behavior is added in Phase 21 (render receives frame by ref) and Phase 22 (dispatch via std::visit).
**Warning signs:** Any member functions in frame structs beyond constructors and trivial accessors.

### Pitfall 6: Forgetting the Draw::TextEdit Dependency
**What goes wrong:** OptionsFrame needs a `Draw::TextEdit editor` field. If `btop_menu_pda.hpp` doesn't include `btop_draw.hpp`, compilation fails.
**Why it happens:** TextEdit is a class with non-trivial constructor, not a POD -- it needs the full definition, not just a forward declaration.
**How to avoid:** Include `btop_draw.hpp` in `btop_menu_pda.hpp`. This is acceptable because btop_draw.hpp is a leaf header (it doesn't include menu headers).
**Warning signs:** "incomplete type" errors when declaring `Draw::TextEdit` as a frame member.

### Pitfall 7: Variant Size Bloat
**What goes wrong:** The variant's size equals the largest alternative. If OptionsFrame is huge (many fields), every slot in the stack pays that cost.
**Why it happens:** std::variant stores all alternatives in-place (no heap allocation per alternative).
**How to avoid:** For Phase 20 this is acceptable -- max stack depth is ~4, and OptionsFrame's fields are mostly ints/bools. Total variant size will be a few hundred bytes at most. If profiling later shows issues, alternatives can be heap-allocated via `std::unique_ptr` inside the variant, but this is premature optimization.
**Warning signs:** Worrying about variant size before measuring. The current code has these fields as statics anyway -- putting them in a variant on a stack of depth 4 is strictly better.

## Code Examples

### Frame Struct Definitions (All 8 Menus)
```cpp
// src/btop_menu_pda.hpp (frame struct section)

#include <bitset>
#include <string>
#include <unordered_map>
#include <vector>

#include "btop_input.hpp"   // Input::Mouse_loc
#include "btop_draw.hpp"    // Draw::TextEdit

namespace menu {

struct MainFrame {
    // Layout
    int y{};
    // Interaction
    int selected{};
    std::vector<std::string> colors_selected{};
    std::vector<std::string> colors_normal{};
    // Mouse
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};

struct OptionsFrame {
    // Layout
    int x{};
    int y{};
    int height{};
    // Interaction
    int page{};
    int pages{};
    int selected{};
    int select_max{};
    int item_height{};
    int selected_cat{};
    int max_items{};
    int last_sel{};
    bool editing{false};
    Draw::TextEdit editor{};
    std::string warnings{};
    std::bitset<8> selPred{};
    // Mouse
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};

struct HelpFrame {
    // Layout
    int x{};
    int y{};
    int height{};
    // Interaction
    int page{};
    int pages{};
    // Mouse
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};

struct SizeErrorFrame {
    // Minimal -- uses shared msgBox (msgBox ownership migrated in Phase 21)
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};

struct SignalChooseFrame {
    // Layout
    int x{};
    int y{};
    // Interaction
    int selected_signal{-1};
    // Mouse
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};

struct SignalSendFrame {
    // Minimal -- uses shared msgBox
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};

struct SignalReturnFrame {
    // Minimal -- uses shared msgBox
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};

struct ReniceFrame {
    // Layout
    int x{};
    int y{};
    // Interaction
    int selected_nice{0};
    std::string nice_edit{};
    // Mouse
    std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings{};
};

} // namespace menu
```

### Static Assertions for Variant Correctness
```cpp
// In test_menu_pda.cpp
#include "btop_menu_pda.hpp"
#include <gtest/gtest.h>

TEST(MenuFrameVar, HasExactlyEightAlternatives) {
    EXPECT_EQ(std::variant_size_v<menu::MenuFrameVar>, 8u);
}

TEST(MenuFrameVar, HoldsMainFrame) {
    menu::MenuFrameVar v = menu::MainFrame{};
    EXPECT_TRUE(std::holds_alternative<menu::MainFrame>(v));
}

// ... repeat for all 8 frame types
```

### PDA Stack Operations Test
```cpp
TEST(MenuPDA, PushIncreasesDepth) {
    menu::MenuPDA pda;
    EXPECT_TRUE(pda.empty());
    pda.push(menu::MainFrame{});
    EXPECT_FALSE(pda.empty());
    EXPECT_EQ(pda.depth(), 1u);
}

TEST(MenuPDA, PopDecreasesDepth) {
    menu::MenuPDA pda;
    pda.push(menu::MainFrame{});
    pda.push(menu::OptionsFrame{});
    EXPECT_EQ(pda.depth(), 2u);
    pda.pop();
    EXPECT_EQ(pda.depth(), 1u);
    EXPECT_TRUE(std::holds_alternative<menu::MainFrame>(pda.top()));
}

TEST(MenuPDA, ReplaceKeepsDepth) {
    menu::MenuPDA pda;
    pda.push(menu::MainFrame{});
    EXPECT_EQ(pda.depth(), 1u);
    pda.replace(menu::OptionsFrame{});
    EXPECT_EQ(pda.depth(), 1u);
    EXPECT_TRUE(std::holds_alternative<menu::OptionsFrame>(pda.top()));
}

TEST(MenuPDA, TopReturnsCorrectFrame) {
    menu::MenuPDA pda;
    pda.push(menu::SignalChooseFrame{.selected_signal = 9});
    auto& frame = std::get<menu::SignalChooseFrame>(pda.top());
    EXPECT_EQ(frame.selected_signal, 9);
}

TEST(MenuPDA, FrameIsolation) {
    menu::MenuPDA pda;
    pda.push(menu::MainFrame{.selected = 2});
    pda.push(menu::OptionsFrame{.page = 3});
    auto& opts = std::get<menu::OptionsFrame>(pda.top());
    EXPECT_EQ(opts.page, 3);
    pda.pop();
    auto& main = std::get<menu::MainFrame>(pda.top());
    EXPECT_EQ(main.selected, 2);  // Main frame preserved
}
```

### PDAAction Enum Test
```cpp
TEST(PDAAction, AllValuesDistinct) {
    EXPECT_NE(menu::PDAAction::NoChange, menu::PDAAction::Push);
    EXPECT_NE(menu::PDAAction::Push, menu::PDAAction::Pop);
    EXPECT_NE(menu::PDAAction::Pop, menu::PDAAction::Replace);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| bitset<8> menuMask + int currentMenu | std::variant-based PDA stack | Phase 20 (this phase) | Type safety, compile-time exhaustiveness, per-frame isolation |
| Function-static locals | Per-frame struct members | Phase 20 (types defined), Phase 21 (migration) | Testable in isolation, no hidden state |
| menuFunc vector<ref> dispatch | std::visit on variant | Phase 22 (future) | Compile-time dispatch, no runtime index errors |
| Shared Menu::mouse_mappings | Per-frame mouse_mappings member | Phase 20 (types defined) | Only top frame's mappings active |
| bg.empty() as init sentinel | In-class member initializers | Phase 20 (types defined) | Unconditional initialization, no sentinel bugs |

**Deprecated/outdated:**
- The current menuMask/currentMenu/menuFunc pattern is not "deprecated" in the C++ ecosystem sense -- it works. But it allows invalid state combinations (e.g., menuMask has bits set for menus that aren't actually stacked) which a typed PDA eliminates by construction.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 (via FetchContent) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cmake --build build-test --target btop_test && ctest --test-dir build-test --output-on-failure` |
| Full suite command | `cmake --build build-test --target btop_test && ctest --test-dir build-test --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PDA-01 | MenuFrameVar variant has 8 alternatives | unit (static assertion) | `ctest --test-dir build-test -R MenuFrameVar --output-on-failure` | Wave 0 |
| PDA-02 | push/pop/replace/top/empty work correctly | unit | `ctest --test-dir build-test -R MenuPDA --output-on-failure` | Wave 0 |
| PDA-03 | PDAAction enum has Push/Pop/Replace/NoChange values | unit (static assertion) | `ctest --test-dir build-test -R PDAAction --output-on-failure` | Wave 0 |
| PDA-05 | bg lifecycle tied to stack depth | unit | `ctest --test-dir build-test -R MenuPDA.BgLifecycle --output-on-failure` | Wave 0 |
| FRAME-02 | Layout/interaction field separation | unit (construction test) | `ctest --test-dir build-test -R Frame --output-on-failure` | Wave 0 |
| FRAME-04 | Constructors unconditionally initialize all fields | unit | `ctest --test-dir build-test -R Frame.DefaultInit --output-on-failure` | Wave 0 |
| FRAME-05 | Per-frame mouse_mappings owned by frame struct | unit | `ctest --test-dir build-test -R Frame.MouseMappings --output-on-failure` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build build-test --target btop_test && ctest --test-dir build-test --output-on-failure`
- **Per wave merge:** Same (no integration tests needed -- Phase 20 is isolated types)
- **Phase gate:** Full suite green before verify-work

### Wave 0 Gaps
- [ ] `tests/test_menu_pda.cpp` -- covers PDA-01, PDA-02, PDA-03, PDA-05, FRAME-02, FRAME-04, FRAME-05
- [ ] `tests/CMakeLists.txt` -- add test_menu_pda.cpp to btop_test sources
- [ ] `src/btop_menu_pda.hpp` -- the header being tested (must exist before tests compile)

*(All gaps are new files created in Phase 20 implementation)*

## Open Questions

1. **Should msgBox become a frame struct member?**
   - What we know: `signalSend`, `signalReturn`, and `sizeError` currently use the shared `Menu::msgBox messageBox` variable. In Phase 20, these frames are minimal stubs.
   - What's unclear: Whether to make msgBox a member of SignalSendFrame/SignalReturnFrame/SizeErrorFrame now (Phase 20) or defer to Phase 21 when the actual migration happens.
   - Recommendation: Defer to Phase 21. Phase 20 defines minimal frames for these three menus. Adding msgBox as a member now would require including more of the btop headers and complicate the struct definitions without providing testability value.

2. **Should PDAAction carry a payload for Push/Replace?**
   - What we know: When `mainMenu` selects Options, it needs to tell the dispatch loop "push an OptionsFrame." The PDAAction::Push enum alone doesn't specify WHICH frame to push.
   - What's unclear: Whether the PDAAction enum should carry the target frame (e.g., `struct PushAction { MenuFrameVar frame; }`) or whether the dispatch loop should infer the target from context.
   - Recommendation: In Phase 20, keep PDAAction as a simple enum. In Phase 22 (dispatch wiring), extend to a `PDAResult` struct or use `std::variant<NoChange, Push{MenuFrameVar}, Pop, Replace{MenuFrameVar}>` if needed. Phase 20 defines the minimal types; Phase 22 evolves them. This avoids over-engineering before the dispatch pattern is concrete.

3. **Include dependency on btop_draw.hpp for Draw::TextEdit**
   - What we know: OptionsFrame needs `Draw::TextEdit editor` as a member. btop_draw.hpp is a fairly large header.
   - What's unclear: Whether including btop_draw.hpp will create compile-time overhead for test builds.
   - Recommendation: Include it. btop_draw.hpp is already included transitively in many compilation units. The test binary already links libbtop which includes all sources. If compile time becomes an issue, TextEdit can be forward-declared and stored via unique_ptr, but this is premature.

## Sources

### Primary (HIGH confidence)
- btop_state.hpp (src/btop_state.hpp) -- existing variant-based FSM pattern used in v1.1
- btop_menu.hpp (src/btop_menu.hpp) -- current menu interface (menuMask, Menus enum, active bool, mouse_mappings)
- btop_menu.cpp (src/btop_menu.cpp) -- current menu implementation (all 8 static functions, static locals, process/show)
- btop_input.hpp (src/btop_input.hpp) -- Mouse_loc struct definition
- btop_input.cpp (src/btop_input.cpp) -- current input routing (Menu::active check, old_filter)
- tests/CMakeLists.txt -- GTest v1.17.0 infrastructure
- tests/test_app_state.cpp -- pattern for testing variant-based state types
- CMakeLists.txt -- C++23 standard, project build configuration

### Secondary (MEDIUM confidence)
- C++ standard library documentation for std::variant, std::stack, std::visit -- well-established C++17/23 features
- PROJECT.md (.planning/PROJECT.md) -- architecture history and constraints
- REQUIREMENTS.md (.planning/REQUIREMENTS.md) -- formal requirement definitions
- ROADMAP.md (.planning/ROADMAP.md) -- phase dependencies and success criteria

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- using only C++ standard library types already proven in this codebase
- Architecture: HIGH -- directly follows the established btop_state.hpp pattern; all 8 menu functions and their static locals are fully catalogued
- Pitfalls: HIGH -- variant self-modification UB, header cycles, and bg sentinel are all documented with prevention strategies derived from reading the actual current code

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable -- C++ standard library, no external dependencies to go stale)
