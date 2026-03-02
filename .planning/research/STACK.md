# Stack Research

**Domain:** C++ pushdown automaton + input FSM for btop++ TUI (v1.3 milestone)
**Researched:** 2026-03-02
**Confidence:** HIGH

## Context

This file covers only what is **new or changed** for v1.3. The existing v1.0 profiling stack (perf, heaptrack, Instruments, nanobench) and the v1.1/v1.2 FSM stack (std::variant, std::visit, AppEvent, EventQueue) are already validated. Do not re-add those here; they remain in use unchanged.

The v1.3 work has two new implementation surfaces:
1. **Menu pushdown automaton (PDA):** A typed stack of menu frames replacing `menuMask bitset<8>` + `currentMenu int` + function-static locals
2. **Input FSM:** A typed finite state machine for input mode routing replacing the implicit `Config::getB(BoolKey::proc_filtering)` + `Menu::active` flag checks

---

## Recommended Stack

### Core Technologies (NEW for v1.3)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **`std::stack<MenuFrame, std::vector<MenuFrame>>`** | C++11 std, `<stack>` | PDA stack container | `std::stack` with `std::vector` as the backing store gives contiguous memory layout, amortized O(1) push/pop, and zero extra dependencies. The menu stack depth is bounded (max 3-4 levels deep: `Main → Options`, `Main → SignalChoose → SignalSend`, etc.), so `std::deque` (the default backing store) wastes memory on small sizes. `std::vector<MenuFrame>` with a pre-reserved capacity of 8 keeps all frames on a single heap allocation. **Confidence: HIGH** — cppreference confirms `stack<T, vector<T>>` is fully supported; vector backing is the standard recommendation for bounded-depth stacks |
| **`using MenuFrame = std::variant<frame::Main, frame::Options, frame::Help, frame::SignalChoose, frame::SignalSend, frame::SignalReturn, frame::Renice, frame::SizeError>`** | C++17 std, `<variant>` | Typed stack frame union | Identical pattern to the existing `AppStateVar` + `AppEvent` approach already validated in v1.1. Each frame alternative carries its per-frame state (selected item, scroll position, signal target) as data fields — no function-static locals needed. `std::variant` prevents invalid frame combinations at compile time. **Confidence: HIGH** — direct extension of existing codebase pattern |
| **`using InputStateVar = std::variant<input_state::Normal, input_state::Filtering, input_state::MenuActive>`** | C++17 std, `<variant>` | Input FSM state type | Three alternatives matching the three input modes already present implicitly in `Input::process`. `input_state::Filtering` carries `std::string filter_text`; `input_state::MenuActive` carries nothing (menu PDA is the authoritative state); `input_state::Normal` carries nothing. Same pattern as `AppStateVar`. **Confidence: HIGH** |
| **`std::visit` two-variant dispatch** | C++17 std, `<variant>` | State × event transition | The existing `dispatch_event(current, ev)` function uses `std::visit([](const auto& s, const auto& e){ return on_event(s, e); }, current, ev)` — identical pattern applies to the input FSM. Compiler generates an exhaustive switch over all state × event combinations. **Confidence: HIGH** — already proven in this codebase |
| **Overload pattern (C++17/20)** | C++17 required, C++20 simplifies | Inline visitor construction | `template<class... Ts> struct overload : Ts... { using Ts::operator()...; };` — used for single-variant `std::visit` calls where a concise inline lambda set is cleaner than named `on_event` overloads. In C++20, the explicit deduction guide is no longer needed. For the PDA's `peek()` + dispatch pattern, this is cleaner than a separate `on_event` overload set when there are only 2-3 frame alternatives to handle. **Confidence: HIGH** — standard C++17/20 idiom, verified in C++ Stories and cppreference |
| **`if constexpr` + `std::is_same_v`** | C++17 std | Type-conditional logic in generic visitors | Used inside `auto`-parameter lambdas to handle frame-specific rendering or key handling without virtual dispatch: `if constexpr (std::is_same_v<T, frame::Options>) { ... }`. Already used in `to_tag()` in `btop_state.hpp`. **Confidence: HIGH** |

### Supporting Libraries (existing, no changes)

| Library | Version | Purpose | Notes for v1.3 |
|---------|---------|---------|----------------|
| **GoogleTest** | v1.17.0 (FetchContent) | Unit tests for PDA and input FSM | Already in CMakeLists.txt. New test files `test_menu_pda.cpp` and `test_input_fsm.cpp` added to `btop_test` target. No version change needed. |
| **nanobench** | 4.3+ (header-only) | Micro-benchmarks for push/pop/dispatch | Already present. Optional: benchmark PDA dispatch cost vs old `menuMask` check if needed. |

### Development Tools (existing, no changes)

| Tool | Purpose | Notes |
|------|---------|-------|
| **ASan + UBSan** | Detect use-after-pop, variant bad_access | Build config `build-asan/` already exists. Run after PDA implementation. |
| **TSan** | Verify menu PDA is main-thread-only (no races) | `build-tsan/` already exists. PDA should be touched only from main thread — TSan confirms this. |

---

## Installation

No new dependencies for v1.3. All required headers (`<stack>`, `<variant>`, `<type_traits>`) are part of the C++ standard library already included in the build.

```bash
# Nothing to install — all technologies are:
# 1. C++ standard library (GCC 12+ / Clang 15+, already required)
# 2. Already present dependencies (GoogleTest v1.17.0 via FetchContent, nanobench)
```

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| `std::stack<MenuFrame, std::vector<MenuFrame>>` | `std::stack<MenuFrame>` (default deque) | Never for this use case. `std::deque` is the right choice when depth is unbounded or element size is large enough that deque's block-based layout avoids reallocs. For 8 menus with ≤4 simultaneous levels, `vector` backing is strictly better (contiguous, fewer allocations, cache-friendly). |
| `std::variant` for `MenuFrame` | Virtual base class `MenuFrameBase*` | Only if frames need runtime polymorphism not expressible as data fields, or if adding new frame types at runtime is required. Neither applies here — frame set is fixed at 8. Virtual dispatch adds heap allocation, pointer indirection, and prevents value semantics. The existing codebase explicitly chose `std::variant` over virtual dispatch for `AppStateVar`; maintain consistency. |
| `std::variant` for `InputStateVar` | `enum class InputMode` (flat enum) | Only if per-state data (like `filter_text` in `input_state::Filtering`) is stored as a separate global rather than inside the state. Using a plain enum recreates the same implicit-state problem v1.3 is solving. `std::variant` carries state with the mode, making invalid configurations impossible. |
| Named `on_event` overloads for input FSM dispatch | `if/else if` chain in `Input::process` | If the FSM has more than ~6 transitions. For 3 states × ~5 events, the existing `if/else if` with an `on_event` overload set is readable. For fewer branches, the overload pattern with inline lambdas is acceptable. Choose whichever matches what is already written for `dispatch_event`. |
| Two separate test files (`test_menu_pda.cpp`, `test_input_fsm.cpp`) | Single combined test file | Only if the test count stays under ~30. At the scale of the existing test files (266 total), separate files are easier to navigate and build independently. |
| GoogleTest `TYPED_TEST` for testing multiple frame types | Separate `TEST()` per frame | `TYPED_TEST` is appropriate when the same test logic applies to all types in a list. For PDA tests, the transitions between frames are heterogeneous enough that separate named tests are clearer than typed parameterization. Use `TEST_P` + `INSTANTIATE_TEST_SUITE_P` only for table-driven transition tests where input is `(initial_frames, push_or_pop, expected_top)` tuples. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **Virtual dispatch (`MenuFrameBase*` polymorphism)** | Heap allocation per frame, pointer indirection, no value semantics, breaks stack copy/move cleanly. The existing codebase explicitly avoided virtual dispatch for FSMs. | `std::variant` frame type — zero-cost, value semantics, compile-time exhaustiveness |
| **`bitset<8> menuMask` + `int currentMenu`** (the thing being replaced) | Allows invalid state (multiple bits set, currentMenu inconsistent with mask, function-static locals scattered across menus). Cannot test individual menus in isolation because state is global and implicit. | `std::stack<MenuFrame, std::vector<MenuFrame>>` — exactly one active frame at stack top, each frame carries its own data |
| **`Config::getB(BoolKey::proc_filtering)` + `Menu::active` checks scattered in `Input::process`** (the thing being replaced) | Implicit mode routing — behavior depends on two separate global flags checked in sequence. Adding a new mode requires hunting for all check sites. | `InputStateVar` FSM with `dispatch_event` — mode is a single typed value; adding a new mode requires adding one variant alternative and one set of `on_event` overloads |
| **`std::deque` as PDA backing store** | `std::deque` allocates in blocks; for ≤8 elements of variant size (~100 bytes for a frame carrying a string + ints), deque adds overhead with no benefit. | `std::stack<MenuFrame, std::vector<MenuFrame>>` — single contiguous allocation, reserved at construction |
| **Third-party FSM libraries (Boost.MSM, TinyFSM, etc.)** | Add build dependencies; btop's constraint is "no new heavy dependencies." The existing `std::variant` pattern has already been proven at 266 tests. Third-party libraries add concepts and macros that conflict with the codebase style. | Bare `std::variant` + `std::visit` + `std::stack` — proven pattern already in this codebase |
| **`std::stack<std::any>`** | `std::any` erases type; frame access requires `std::any_cast` which throws on mismatch, producing runtime rather than compile-time errors. Eliminates all type-safety benefits. | `std::variant<frame::Main, frame::Options, ...>` — compile-time exhaustiveness, no cast required |
| **Storing mouse mappings as function-static locals inside `Menu::process()`** | Already identified as a problem in the existing code. Function-static locals survive menu dismissal, creating stale mappings when menus are re-entered. | Per-frame data field in the `MenuFrame` variant struct. Each frame alternative carries its own `std::unordered_map<std::string, Input::Mouse_loc> mouse_mappings` (or equivalent). Field is initialized on push, destroyed on pop. |

---

## Stack Patterns for This Milestone

**Menu PDA stack declaration:**
```cpp
// In btop_menu.hpp (or btop_state.hpp)
namespace menu_frame {
    struct Main    { int selected{0}; };
    struct Options { int selected{0}; int scroll{0}; int category{0}; };
    struct Help    { int scroll{0}; };
    struct SignalChoose { int selected{0}; };
    struct SignalSend   { int signal{0}; int pid{0}; };
    struct SignalReturn {};
    struct Renice  { int selected{0}; int nice{0}; };
    struct SizeError {};
}

using MenuFrame = std::variant<
    menu_frame::Main,
    menu_frame::Options,
    menu_frame::Help,
    menu_frame::SignalChoose,
    menu_frame::SignalSend,
    menu_frame::SignalReturn,
    menu_frame::Renice,
    menu_frame::SizeError
>;

// PDA stack — main-thread only
// Use vector backing for bounded depth; reserve(8) avoids realloc
class MenuPDA {
    std::stack<MenuFrame, std::vector<MenuFrame>> frames_;
public:
    MenuPDA() { frames_.c.reserve(8); }

    void push(MenuFrame f) { frames_.push(std::move(f)); }
    void pop()             { if (!frames_.empty()) frames_.pop(); }
    bool empty()     const { return frames_.empty(); }
    bool active()    const { return !frames_.empty(); }

    const MenuFrame& top() const { return frames_.top(); }
    MenuFrame&       top()       { return frames_.top(); }
};
```

**Input FSM state declaration:**
```cpp
// In btop_input.hpp (or a new btop_input_state.hpp)
namespace input_state {
    struct Normal   {};
    struct Filtering { std::string filter_text; };
    struct MenuActive {};   // Menu PDA is the authoritative state
}

using InputStateVar = std::variant<
    input_state::Normal,
    input_state::Filtering,
    input_state::MenuActive
>;
```

**Dispatch pattern (mirrors existing dispatch_event):**
```cpp
// In btop_input.cpp
static InputStateVar on_event(const input_state::Normal&, const input_event::StartFilter&) {
    return input_state::Filtering{};
}
static InputStateVar on_event(const input_state::Filtering& s, const input_event::KeyChar& e) {
    auto next = s;
    next.filter_text += e.ch;
    return next;
}
static InputStateVar on_event(const auto& s, const auto&) {
    return s; // catch-all: unhandled pairs preserve state
}

InputStateVar dispatch_input(const InputStateVar& current, const InputEvent& ev) {
    return std::visit(
        [](const auto& state, const auto& event) -> InputStateVar {
            return on_event(state, event);
        },
        current, ev
    );
}
```

**PDA dispatch (overload pattern for per-frame rendering):**
```cpp
// In btop_menu.cpp
void render_top_frame(const MenuFrame& frame) {
    std::visit(overload{
        [](const menu_frame::Main& f)    { /* render main menu, use f.selected */ },
        [](const menu_frame::Options& f) { /* render options, use f.selected, f.scroll */ },
        [](const menu_frame::Help& f)    { /* render help, use f.scroll */ },
        [](const auto&)                  { /* other frames */ },
    }, frame);
}
```

**Overload helper (C++20, no deduction guide needed):**
```cpp
// In btop_tools.hpp or inline at use site
template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
// C++20: deduction guide is implicit — no extra line needed
```

**If [C++23 static_assert safety is desired]:**
```cpp
// Catch-all that errors at compile time if a frame type is not handled
template<class... Ts> struct overload : Ts... {
    using Ts::operator()...;
    consteval void operator()(auto) const {
        static_assert(false, "Unhandled MenuFrame alternative in visitor");
    }
};
```
Use the exhaustive version when adding new frame types is expected (forces callers to handle all alternatives). Use the catch-all `on_event` version when a no-op default is intentional (as in the existing codebase).

---

## Testing Approach for Stack-Based State Machines

**What to test in `test_menu_pda.cpp`:**

```cpp
// 1. Empty PDA is inactive
TEST(MenuPDA, EmptyIsInactive) { MenuPDA pda; EXPECT_FALSE(pda.active()); }

// 2. Push makes active, top() returns pushed frame
TEST(MenuPDA, PushActivates) {
    MenuPDA pda;
    pda.push(menu_frame::Main{});
    EXPECT_TRUE(pda.active());
    EXPECT_TRUE(std::holds_alternative<menu_frame::Main>(pda.top()));
}

// 3. Push-then-pop returns to empty (not to previous frame)
TEST(MenuPDA, PushPopIsEmpty) {
    MenuPDA pda;
    pda.push(menu_frame::Main{});
    pda.pop();
    EXPECT_FALSE(pda.active());
}

// 4. Stack preserves frame-below after push/pop sequence
TEST(MenuPDA, StackPreservesUnderneath) {
    MenuPDA pda;
    pda.push(menu_frame::Main{.selected = 2});
    pda.push(menu_frame::Help{});
    pda.pop();
    ASSERT_TRUE(std::holds_alternative<menu_frame::Main>(pda.top()));
    EXPECT_EQ(std::get<menu_frame::Main>(pda.top()).selected, 2);
}

// 5. Per-frame data survives round-trip (no mutation on push/pop)
TEST(MenuPDA, FrameDataPreserved) {
    MenuPDA pda;
    pda.push(menu_frame::Options{.selected = 3, .scroll = 10, .category = 1});
    const auto& top = std::get<menu_frame::Options>(pda.top());
    EXPECT_EQ(top.selected, 3);
    EXPECT_EQ(top.scroll, 10);
}

// 6. Table-driven transition test using TEST_P
struct PdaTransition { MenuFrame push_frame; MenuFrame expected_top; };
class PdaTransitionTest : public ::testing::TestWithParam<PdaTransition> {};
TEST_P(PdaTransitionTest, TopMatchesExpected) {
    MenuPDA pda;
    pda.push(GetParam().push_frame);
    EXPECT_EQ(pda.top().index(), GetParam().expected_top.index());
}
INSTANTIATE_TEST_SUITE_P(AllFrameTypes, PdaTransitionTest, ::testing::Values(
    PdaTransition{menu_frame::Main{},        menu_frame::Main{}},
    PdaTransition{menu_frame::Options{},     menu_frame::Options{}},
    PdaTransition{menu_frame::Help{},        menu_frame::Help{}},
    PdaTransition{menu_frame::SignalChoose{}, menu_frame::SignalChoose{}}
));
```

**What to test in `test_input_fsm.cpp`:**
- Each `on_event(state, event)` overload returns the expected next state
- Catch-all `on_event(auto, auto)` returns current state unchanged
- `dispatch_input(current, ev)` routes correctly (mirrors `test_transitions.cpp`)
- `input_state::Filtering` carries `filter_text` across multiple `KeyChar` events

**What NOT to test (avoid):**
- Rendering logic (involves terminal I/O, not suitable for unit test)
- Mouse mapping resolution (depends on terminal geometry)
- Any code path that calls `Menu::show()` or `Input::process()` directly (integration concern, not unit concern)

---

## Version Compatibility

| Component | Version Requirement | Notes |
|-----------|---------------------|-------|
| `std::stack<T, std::vector<T>>` | C++11 | Standard since C++11; fully available in GCC 12+ / Clang 15+ |
| `std::variant` | C++17 | Already used in btop_state.hpp and btop_events.hpp |
| `std::visit` two-variant form | C++17 | Already used in `dispatch_event` in btop.cpp |
| Overload pattern (no deduction guide) | C++20 | GCC 12 and Clang 15 both support C++20 CTAD for aggregates |
| `consteval` static_assert safety | C++23 | Optional enhancement. GCC 12 supports C++23 with `-std=c++23`; Clang 15 supports C++23. Only use if the project's CMakeLists.txt is already targeting C++23. |
| GoogleTest v1.17.0 `TEST_P` / `INSTANTIATE_TEST_SUITE_P` | Requires C++17 (GTest 1.17 requires C++17) | Already in CMakeLists.txt at v1.17.0 |

---

## Sources

- [cppreference — std::stack](https://en.cppreference.com/w/cpp/container/stack) — Confirmed `stack<T, vector<T>>` parameterization, required container operations. **HIGH confidence.**
- [cppreference — std::variant::visit](https://en.cppreference.com/w/cpp/utility/variant/visit) — Two-variant `std::visit` form confirmed C++17. Member function overloads are C++26 (not applicable here). **HIGH confidence.**
- [C++ Stories — Overload Pattern](https://www.cppstories.com/2019/02/2lines3featuresoverload.html/) — Overload pattern in C++17/20, C++20 eliminates deduction guide, C++23 `consteval` catch-all for exhaustive matching. **HIGH confidence.**
- [C++ Stories — FSM with std::variant (2023)](https://www.cppstories.com/2023/finite-state-machines-variant-cpp/) — Two-variant `std::visit` for state × event dispatch, vending machine example mirrors btop's `dispatch_event` pattern. **HIGH confidence.**
- [Game Programming Patterns — State chapter](https://gameprogrammingpatterns.com/state.html) — Pushdown automaton for menu/game-state management: stack of states with push/pop instead of single-state replacement. Confirms the "stack of typed frames" pattern. **MEDIUM confidence** (blog, but canonical reference for the pattern).
- `btop_state.hpp`, `btop_events.hpp`, `btop.cpp` — Direct codebase analysis. The `AppStateVar`, `AppEvent`, `dispatch_event`, `on_event`, `transition_to` patterns in the existing code are the direct model for v1.3. **HIGH confidence** (primary source).
- `tests/CMakeLists.txt`, `tests/test_transitions.cpp` — Direct codebase analysis. GoogleTest v1.17.0, `TEST()` pattern, `holds_alternative` + `get` assertions. **HIGH confidence** (primary source).
- [GoogleTest v1.17.0 — Testing Reference](https://google.github.io/googletest/reference/testing.html) — `TEST_P`, `INSTANTIATE_TEST_SUITE_P` for table-driven transition tests. **HIGH confidence.**

---

*Stack research for: btop++ v1.3 Menu PDA + Input FSM*
*Researched: 2026-03-02*
