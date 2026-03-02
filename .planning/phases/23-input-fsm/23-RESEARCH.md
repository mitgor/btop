# Phase 23: Input FSM - Research

**Researched:** 2026-03-02
**Domain:** C++ std::variant-based finite state machine for input routing; removal of boolean-flag dispatch in a terminal UI application
**Confidence:** HIGH

## Summary

Phase 23 introduces an InputStateVar std::variant with three states (Normal, Filtering, MenuActive) that becomes the sole authority for how keyboard and mouse input is routed. Currently, input routing is split across two separate boolean-flag checks: `Config::getB(BoolKey::proc_filtering)` controls whether keystrokes are forwarded to the filter text editor vs. normal box handlers, and `Menu::active` controls whether keys go to `Menu::process()` vs. `Input::process()`. The main loop at btop.cpp:1572-1574 uses `if (Menu::active) Menu::process(key); else Input::process(key);` as the top-level dispatch, while inside Input::process() a second check on `filtering` further branches. Phase 23 replaces all of this with a single std::visit dispatch on InputStateVar.

The phase has four distinct concerns: (1) Define the InputStateVar variant and its three state structs, with Filtering carrying `old_filter` as a member. (2) Rewrite Input::process() to dispatch via the FSM instead of boolean checks. (3) Rewrite the mouse routing in Input::get() to read InputStateVar instead of Menu::active. (4) Wire the integration points: App FSM on_enter(Resizing) must call pda.invalidate_layout(), and Runner::pause_output must be cleared on all paths that empty the PDA stack. The integration concerns (INTEG-01 through INTEG-04) are straightforward wiring changes since the PDA already exists from Phases 20-22.

This phase eliminates `Menu::active` entirely (the atomic bool declared in btop_menu.hpp), replaces the file-scoped `old_filter` global in btop_input.cpp with a struct member, and moves the main-loop dispatch from btop.cpp into a FSM-based router. Config::getB(BoolKey::proc_filtering) is retained for display/persistence (it drives the filter UI rendering in btop_draw.cpp) but is no longer the routing authority.

**Primary recommendation:** Define InputStateVar in btop_input.hpp with Normal/Filtering/MenuActive structs. Filtering carries old_filter as a std::string member. Add a file-scoped InputStateVar in btop_input.cpp. Rewrite process() with std::visit dispatch. Replace the btop.cpp main-loop if/else with a call to a unified Input::process() that internally routes to Menu::process() when in MenuActive state. Remove Menu::active atomic bool.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INPUT-01 | InputStateVar std::variant defined with Normal, Filtering, and MenuActive states | std::variant with three struct alternatives, following the same pattern as AppStateVar in btop_state.hpp and MenuFrameVar in btop_menu_pda.hpp |
| INPUT-02 | Filtering state carries old_filter as struct member (replacing file-scoped global) | Filtering struct has a std::string old_filter member; the file-scoped `string old_filter` at btop_input.cpp:96 is deleted |
| INPUT-03 | All input transitions typed: Normal<->Filtering, Normal<->MenuActive | Transitions are function calls that replace the InputStateVar value; entering Filtering saves old_filter in the struct, exiting restores it |
| INPUT-04 | Input::process() dispatches via InputStateVar instead of if(filtering)/if(Menu::active) branches | std::visit with three operator() overloads, or std::get_if chain, dispatches to normal/filtering/menu handlers |
| INPUT-05 | Menu::active atomic bool removed -- InputStateVar is sole authority for input routing | All 8 reads of Menu::active (btop.cpp, btop_menu.cpp, btop_draw.cpp, btop_input.cpp) replaced with InputStateVar queries or removed |
| INPUT-06 | Mouse routing in Input::get() reads InputStateVar instead of Menu::active bool | The ternary at btop_input.cpp:182 `(Menu::active ? Menu::mouse_mappings : mouse_mappings)` replaced with InputStateVar check |
| INTEG-01 | App FSM on_enter(Resizing) calls menu_pda.invalidate_layout() | on_enter(state::Resizing&) at btop.cpp:1004 already calls Menu::process() when active; add pda.invalidate_layout() call. Requires exposing pda or adding a Menu:: wrapper function |
| INTEG-02 | Runner::pause_output cleared on all paths that empty the PDA stack | Already handled in Menu::process() at lines 1831, 1882, 1893; this requirement verifies the existing behavior is preserved after FSM changes |
| INTEG-03 | Config::proc_filtering retained for display/persistence but FSM is routing authority | btop_draw.cpp:2382 reads proc_filtering for UI rendering; this remains. Input routing reads InputStateVar instead |
| INTEG-04 | Main loop btop.cpp:1572-1574 replaced with InputFSM-based dispatch | The `if (Menu::active) Menu::process(key); else Input::process(key);` replaced with a single call that dispatches internally via InputStateVar |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| std::variant | C++17 (C++23 project) | InputStateVar type-safe union of Normal/Filtering/MenuActive | Same pattern used for AppStateVar and MenuFrameVar; compile-time exhaustive matching |
| std::visit | C++17 (C++23 project) | Dispatch on InputStateVar for input routing | Compile-time exhaustive; adding a state without a handler is a compile error |
| std::string | C++ stdlib | old_filter member in Filtering state | Replaces file-scope global with struct-owned data |
| GTest | v1.17.0 | Unit tests for InputStateVar types and transitions | Already in project via FetchContent |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::holds_alternative | C++17 | Quick state queries (e.g., "are we in MenuActive?") | For code that needs to check state without full dispatch |
| std::get_if | C++17 | Conditional state access with null safety | For code that conditionally accesses state data (e.g., getting old_filter from Filtering) |
| fmt::format | project dep | String formatting | Already in use throughout the codebase |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| std::variant FSM | enum + switch | enum cannot carry per-state data (old_filter); variant enforces exactly one active state with typed data |
| std::visit dispatch | std::holds_alternative chain | visit is exhaustive at compile time; holds_alternative chain can silently miss a new state |
| Struct member old_filter | std::optional<std::string> | old_filter is always present when in Filtering state; optional adds unnecessary nullability |

## Architecture Patterns

### Recommended Project Structure
```
src/
  btop_input.hpp       # MODIFIED: add InputStateVar variant, Normal/Filtering/MenuActive structs
  btop_input.cpp       # MODIFIED: file-scope InputStateVar, rewritten process(), updated get()
  btop_menu.hpp        # MODIFIED: remove Menu::active declaration
  btop_menu.cpp        # MODIFIED: remove Menu::active definition, reads; add Menu::invalidate_layout() wrapper
  btop.cpp             # MODIFIED: replace if(Menu::active) dispatch; update on_enter(Resizing)
  btop_draw.cpp        # MODIFIED: replace Menu::active reads with InputStateVar queries
tests/
  test_menu_pda.cpp    # EXISTING: PDA tests unchanged
  (Phase 24 adds test_input_fsm.cpp)
```

### Pattern 1: InputStateVar Variant Definition
**What:** A std::variant with three struct alternatives representing all input routing states.
**When to use:** Defined in btop_input.hpp, instantiated as file-scope variable in btop_input.cpp.
**Example:**
```cpp
// In btop_input.hpp, inside namespace Input
namespace input_state {
    struct Normal {};

    struct Filtering {
        std::string old_filter;  // Saved filter text for ESC restoration
    };

    struct MenuActive {};
}

using InputStateVar = std::variant<
    input_state::Normal,
    input_state::Filtering,
    input_state::MenuActive
>;
```

**Key insight:** Normal and MenuActive carry no data. Filtering carries old_filter, which is the only per-state datum. This mirrors the existing code where `old_filter` is set on entering filtering and read on ESC.

### Pattern 2: Transition Functions
**What:** Named functions that perform typed state transitions, replacing boolean flag toggles.
**When to use:** Called from the same places that currently flip Config::proc_filtering or set Menu::active.
**Example:**
```cpp
// In btop_input.cpp
namespace Input {
    InputStateVar fsm_state{input_state::Normal{}};

    void enter_filtering() {
        auto old = Proc::filter.text;
        Proc::filter = Draw::TextEdit{Config::getS(StringKey::proc_filter)};
        fsm_state = input_state::Filtering{Proc::filter.text};
        Config::set(BoolKey::proc_filtering, true);  // Keep for display
    }

    void exit_filtering(bool accept) {
        if (auto* f = std::get_if<input_state::Filtering>(&fsm_state)) {
            if (!accept) {
                Config::set(StringKey::proc_filter, f->old_filter);
            }
            Config::set(BoolKey::proc_filtering, false);
        }
        fsm_state = input_state::Normal{};
    }

    void enter_menu() {
        fsm_state = input_state::MenuActive{};
    }

    void exit_menu() {
        fsm_state = input_state::Normal{};
    }
}
```

### Pattern 3: Unified Input Dispatch
**What:** A single entry point that dispatches input to the correct handler based on FSM state.
**When to use:** Replaces the if/else chain at btop.cpp:1572-1574.
**Example:**
```cpp
// In btop_input.cpp
void Input::process(const std::string_view key) {
    if (key.empty()) return;
    std::visit([&](auto& state) {
        using T = std::decay_t<decltype(state)>;
        if constexpr (std::is_same_v<T, input_state::MenuActive>) {
            Menu::process(key);
            // After Menu::process, check if PDA emptied (menu closed)
            // If so, transition back to Normal
        } else if constexpr (std::is_same_v<T, input_state::Filtering>) {
            process_filtering(key, state);
        } else {
            process_normal(key);
        }
    }, fsm_state);
}
```

### Pattern 4: Mouse Routing via FSM
**What:** Replace the Menu::active ternary in get() with an InputStateVar check.
**When to use:** In Input::get() at the mouse event lookup.
**Example:**
```cpp
// In Input::get(), replacing line 182
const auto& mappings = std::holds_alternative<input_state::MenuActive>(fsm_state)
    ? Menu::mouse_mappings
    : mouse_mappings;
for (const auto& [mapped_key, pos] : mappings) {
    // ... existing hit-test logic
}
```

### Anti-Patterns to Avoid
- **Dual authority:** Do NOT keep both Menu::active and InputStateVar as authorities. Menu::active must be fully removed, not just shadowed.
- **Config as routing authority:** Config::getB(BoolKey::proc_filtering) must NOT be read for routing decisions after this phase. It remains only for display purposes in btop_draw.cpp.
- **Exposing PDA globally:** Do not move the file-scope PDA to a header. Instead, add wrapper functions (e.g., `Menu::is_active()`, `Menu::invalidate_layout()`) if external code needs to query menu state.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| State queries from btop.cpp | Direct PDA access or new global booleans | InputStateVar with std::holds_alternative queries | Single source of truth; no sync issues between multiple flags |
| Menu active check for draw code | Another atomic bool | `Input::is_menu_active()` wrapper reading InputStateVar | Eliminates the dual-authority problem that Menu::active creates |
| Transition validation | Runtime assert chains | Variant type system -- only valid states are representable | Cannot be in Filtering AND MenuActive simultaneously; enforced at compile time |

**Key insight:** The entire point of this phase is to replace hand-rolled boolean flag routing with the type system. Every boolean flag that gates input routing should be replaced by a variant state, not by another boolean flag.

## Common Pitfalls

### Pitfall 1: Menu::active Removal Creates Dangling References
**What goes wrong:** Menu::active is read in 8 locations across 4 files. Missing any reference causes a compile error (good) or, if you replace with a wrapper function, a subtle behavior change.
**Why it happens:** The reads are scattered: btop.cpp (4 uses), btop_menu.cpp (2 uses), btop_draw.cpp (1 use), btop_input.cpp (1 use).
**How to avoid:** Systematic replacement. Each read of Menu::active must be replaced with the appropriate InputStateVar query. Compile with -Werror to catch any missed references.
**Warning signs:** grep for "Menu::active" returns any hits after the phase.

### Pitfall 2: Menu::process() Sets Menu::active Internally
**What goes wrong:** The PDA dispatch loop in btop_menu.cpp explicitly sets `Menu::active = false` (line 1828) and `Menu::active = true` (line 1838). If you remove the declaration but not these writes, compilation fails. If you replace these with InputStateVar transitions but get the ordering wrong, the FSM and PDA can desync.
**Why it happens:** Menu::process() manages its own active state as part of the dispatch loop.
**How to avoid:** Replace `Menu::active = false` (empty stack case) with a call back to Input to transition to Normal. Replace `Menu::active = true` (non-empty stack case) with a no-op (we are already in MenuActive state). The transition to MenuActive happens in show(), not in process().
**Warning signs:** FSM says MenuActive but PDA is empty, or FSM says Normal but PDA has frames.

### Pitfall 3: Filtering Exit Paths Are Complex
**What goes wrong:** There are three exit paths from filtering: (1) Enter/Down accepts filter, (2) ESC/mouse_click rejects filter, and within the accept path, "down" recursively calls process("down") after accepting. Missing any path leaves the FSM stuck in Filtering.
**Why it happens:** The existing code at btop_input.cpp:308-329 has multiple branches.
**How to avoid:** Map all exit paths before coding: enter->accept, down->accept+recurse, escape->reject, mouse_click->reject. Each must call exit_filtering() before any further processing.
**Warning signs:** After pressing ESC in filter mode, the FSM is still in Filtering state.

### Pitfall 4: Config::proc_filtering Must Stay In Sync for Display
**What goes wrong:** btop_draw.cpp:2382 reads Config::getB(BoolKey::proc_filtering) to render the filter UI. If this config value and the FSM state get out of sync, the UI shows the wrong filter state.
**Why it happens:** The FSM transition functions must continue to set Config::proc_filtering for display, even though the FSM is the routing authority.
**How to avoid:** Transition functions (enter_filtering, exit_filtering) always set Config::proc_filtering as a side effect. This is INTEG-03.
**Warning signs:** Filter UI shows active but keys go to normal handlers, or vice versa.

### Pitfall 5: Thread Safety of InputStateVar
**What goes wrong:** Menu::active was atomic<bool> because the runner thread reads it (btop.cpp:906, btop.cpp:625-741 via pause_output checks). InputStateVar is a std::variant which is NOT atomic.
**Why it happens:** std::variant has no atomic specialization in C++.
**How to avoid:** InputStateVar is main-thread only. The runner thread does NOT need to read input state directly -- it reads `Runner::pause_output` (which is atomic<bool>) and `Global::overlay` (which is written by the main thread before `Runner::run()` is called). The runner thread's check at btop.cpp:906 (`if (Menu::active and not current_conf.background_update)`) reads Menu::active from the runner thread. This must be replaced with either (a) pause_output check, or (b) a separate atomic flag, or (c) reading from the already-captured current_conf. Analysis: `current_conf.background_update` is already captured, and `Global::overlay` is already set by the main thread before run(). The check at line 906 clears overlay when menu is active and background_update is false -- this can be replaced with `if (not Global::overlay.empty() and not current_conf.background_update)` since overlay is only non-empty when a menu is active. Alternatively, use `Runner::pause_output` which is already atomic and set when menus are active.
**Warning signs:** TSan reports data race on InputStateVar access from runner thread.

### Pitfall 6: on_enter(Resizing) Must Call invalidate_layout()
**What goes wrong:** Without invalidate_layout(), menu coordinates become stale after terminal resize, causing rendering artifacts or crashes when the menu tries to write to out-of-bounds coordinates.
**Why it happens:** The existing code at btop.cpp:1009 calls `Menu::process()` (with no key) during resize, which triggers a re-render. But it does not invalidate layout first, relying on the bg.empty() sentinel (which was the old pattern).
**How to avoid:** Add `Menu::invalidate_layout()` call in on_enter(Resizing) before the Menu::process() call. Since pda is file-scoped in btop_menu.cpp, add a `Menu::invalidate_layout()` wrapper.
**Warning signs:** Menu renders at wrong position after terminal resize.

### Pitfall 7: The "down" Recursive process() Call in Filtering
**What goes wrong:** When the user presses "down" while filtering, the code accepts the filter, exits filtering mode, then recursively calls `Input::process("down")` to move the selection. With the FSM, the recursive call must see the Normal state (not Filtering).
**Why it happens:** btop_input.cpp:312-317 accepts filter and then calls `process("down")`.
**How to avoid:** Exit filtering (transition to Normal) BEFORE the recursive call. The transition function sets fsm_state to Normal, then the recursive process("down") dispatches via Normal.
**Warning signs:** After pressing "down" in filter mode, the selection does not move.

## Code Examples

### Example 1: InputStateVar Definition
```cpp
// btop_input.hpp — inside namespace Input
namespace input_state {
    struct Normal {};
    struct Filtering {
        std::string old_filter;
    };
    struct MenuActive {};
}

using InputStateVar = std::variant<
    input_state::Normal,
    input_state::Filtering,
    input_state::MenuActive
>;

// Query function for external code (replaces Menu::active reads)
bool is_menu_active();
```

### Example 2: Rewritten Main Loop Dispatch
```cpp
// btop.cpp:1568-1575 — replaces the if(Menu::active) branch
else if (Input::poll(min((uint64_t)1000, running->future_time - current_time))) {
    if (not Runner::is_active()) Config::unlock();
    auto key = Input::get();
    if (not key.empty()) {
        Input::process(key);  // Single entry point; FSM dispatches internally
    }
}
```

### Example 3: Menu::process() Without Menu::active Writes
```cpp
// btop_menu.cpp — process() loop empty-stack case
if (pda.empty()) {
    // Menu::active = false;  // DELETED — caller transitions FSM
    Global::overlay.clear();
    Global::overlay.shrink_to_fit();
    Runner::pause_output.store(false);
    pda.clear_bg();
    mouse_mappings.clear();
    Runner::run("all", true, true);
    return;  // Caller (Input::process) transitions FSM to Normal
}

// Non-empty stack case:
// Menu::active = true;  // DELETED — FSM is already in MenuActive
```

### Example 4: on_enter(Resizing) with invalidate_layout
```cpp
static void on_enter(state::Resizing&, TransitionCtx&) {
    term_resize(true);
    Draw::calcSizes();
    Runner::screen_buffer.resize(Term::width, Term::height);
    Draw::update_clock(true);
    if (Input::is_menu_active()) {
        Menu::invalidate_layout();  // INTEG-01: prevent stale coordinates
        Menu::process();
    }
    else Runner::run("all", true, true);
    // ... rest unchanged
}
```

### Example 5: Filtering Transition Functions
```cpp
// btop_input.cpp
void enter_filtering() {
    Proc::filter = Draw::TextEdit{Config::getS(StringKey::proc_filter)};
    fsm_state = input_state::Filtering{Proc::filter.text};
    Config::set(BoolKey::proc_filtering, true);
}

void exit_filtering(bool accept) {
    if (auto* f = std::get_if<input_state::Filtering>(&fsm_state)) {
        if (accept) {
            Config::set(StringKey::proc_filter, Proc::filter.text);
        } else {
            Config::set(StringKey::proc_filter, f->old_filter);
        }
    }
    Config::set(BoolKey::proc_filtering, false);
    fsm_state = input_state::Normal{};
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Menu::active atomic bool + Config::proc_filtering bool | InputStateVar std::variant | Phase 23 (this phase) | Single source of truth for input routing; impossible to be in two states simultaneously |
| File-scope old_filter global | Filtering struct member | Phase 23 (this phase) | old_filter lifetime tied to Filtering state; automatically cleaned up on transition |
| btop.cpp if/else dispatch | Single Input::process() entry point | Phase 23 (this phase) | Main loop simplified; input routing logic centralized in btop_input.cpp |
| Menu::active read from runner thread | Runner::pause_output or overlay check | Phase 23 (this phase) | Eliminates cross-thread read of non-atomic variant |

## Open Questions

1. **Where should InputStateVar live (header vs. source)?**
   - What we know: AppStateVar is declared in btop_state.hpp and instantiated in btop.cpp. The PDA is file-scoped in btop_menu.cpp. InputStateVar needs to be read from btop.cpp (main loop), btop_input.cpp (process/get), btop_draw.cpp (filter UI), and btop_menu.cpp (active flag replacement).
   - What's unclear: Whether to expose the variant directly via extern or provide wrapper functions.
   - Recommendation: Define the type in btop_input.hpp. Instantiate in btop_input.cpp as file-scope. Provide query functions: `Input::is_menu_active()`, `Input::is_filtering()` for external reads. Transition functions (`enter_menu`, `exit_menu`, `enter_filtering`, `exit_filtering`) exposed only as needed. This matches the PDA pattern (file-scope with wrappers).

2. **Should Menu::show() transition the FSM, or should Input code do it?**
   - What we know: Menu::show() is called from Input::process() (normal key handling). It currently sets Menu::active indirectly through the dispatch loop.
   - What's unclear: Whether Menu::show() should call Input::enter_menu() or if Input::process() should transition before calling Menu::show().
   - Recommendation: Input::process() transitions to MenuActive before calling Menu::show(). Menu::process() does NOT manage FSM state. When Menu::process() returns and the PDA is empty, Input::process() transitions back to Normal. This keeps FSM ownership in one place.

3. **btop.cpp:906 runner thread Menu::active read**
   - What we know: The runner thread reads Menu::active at btop.cpp:906 to decide whether to clear Global::overlay. This is a cross-thread read.
   - What's unclear: Whether replacing with `!Global::overlay.empty()` is semantically equivalent.
   - Recommendation: Replace `if (Menu::active and not current_conf.background_update) Global::overlay.clear();` with `if (Runner::pause_output.load() and not current_conf.background_update) Global::overlay.clear();`. Runner::pause_output is already atomic and is set/cleared in sync with menu active state. This avoids needing InputStateVar to be atomic.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build && ctest --test-dir tests -R InputState --output-on-failure` |
| Full suite command | `cd build && ctest --test-dir tests --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| INPUT-01 | InputStateVar variant has 3 alternatives | unit | `ctest -R InputStateVar` | No - Wave 0 (Phase 24) |
| INPUT-02 | Filtering state carries old_filter | unit | `ctest -R FilteringState` | No - Wave 0 (Phase 24) |
| INPUT-03 | Typed transitions Normal<->Filtering, Normal<->MenuActive | unit | `ctest -R InputTransition` | No - Wave 0 (Phase 24) |
| INPUT-04 | process() dispatches via FSM | integration | manual-only (requires full app context) | N/A |
| INPUT-05 | Menu::active removed | compile-time | `grep -r "Menu::active" src/` returns 0 | N/A |
| INPUT-06 | Mouse routing reads InputStateVar | integration | manual-only (requires terminal input) | N/A |
| INTEG-01 | on_enter(Resizing) calls invalidate_layout() | integration | manual-only (requires resize) | N/A |
| INTEG-02 | pause_output cleared on PDA empty | unit | `ctest -R PauseOutput` | No - Phase 24 |
| INTEG-03 | Config::proc_filtering retained for display | integration | manual-only (requires UI) | N/A |
| INTEG-04 | Main loop replaced with FSM dispatch | integration | manual-only (requires full app) | N/A |

### Sampling Rate
- **Per task commit:** `cmake --build build && cd build && ctest --test-dir tests --output-on-failure`
- **Per wave merge:** Full suite + grep verification for Menu::active removal
- **Phase gate:** Full suite green + `grep -r "Menu::active" src/` returns 0 + manual testing of all menus, filtering, resize

### Wave 0 Gaps
- Note: Phase 24 owns test creation. Phase 23 focuses on implementation. Compile-time verification (grep for Menu::active) and build success are the primary Phase 23 gates.
- Type-level tests for InputStateVar can be added to tests/test_menu_pda.cpp or a new test_input_fsm.cpp in Phase 24.

## Sources

### Primary (HIGH confidence)
- Source code analysis: btop_input.cpp, btop_input.hpp, btop_menu.cpp, btop_menu.hpp, btop.cpp, btop_draw.cpp, btop_state.hpp, btop_menu_pda.hpp
- Phase 20-22 research and plans: .planning/phases/20-pda-types-skeleton/20-RESEARCH.md, .planning/phases/22-pda-dispatch/22-RESEARCH.md, .planning/phases/22-pda-dispatch/22-02-PLAN.md
- Existing tests: tests/test_menu_pda.cpp (492 lines of PDA tests, Phase 20)

### Secondary (MEDIUM confidence)
- C++17 std::variant/std::visit specification: well-established, project already uses extensively
- GTest v1.17.0 patterns: already in use in project

### Tertiary (LOW confidence)
- None. All findings derived from direct source code analysis.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - same std::variant/std::visit pattern used in Phases 20-22; well-proven in this codebase
- Architecture: HIGH - direct analysis of all touch points; clear 1:1 mapping from boolean flags to variant states
- Pitfalls: HIGH - all pitfalls derived from concrete code analysis (line numbers verified); thread safety concern verified against actual runner thread code
- Integration: HIGH - INTEG-01 through INTEG-04 are small, well-defined wiring changes with clear code locations

**Research date:** 2026-03-02
**Valid until:** 2026-04-01 (stable C++ project, no external dependencies changing)
