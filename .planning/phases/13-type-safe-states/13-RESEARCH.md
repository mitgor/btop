# Phase 13: Type-Safe States - Research

**Researched:** 2026-02-28
**Domain:** Replacing AppState enum with std::variant carrying per-state data, with entry/exit actions on transitions
**Confidence:** HIGH

## Summary

Phase 13 graduates btop's `AppState` from an `enum class` (Phase 10) to a `std::variant` of typed state structs. This is the core type-safety transformation of the v1.1 automata architecture: after this phase, being in two states simultaneously is a compile-time error (variant holds exactly one alternative), each state carries its own data (Running holds timing info, Error holds an error message, Quitting holds an exit code), and entry/exit actions fire automatically on state transitions (e.g., `Draw::calcSizes()` on entering Resizing, timer reset on entering Running).

The primary technical challenge is that the current `atomic<AppState>` is read from multiple threads (runner thread, draw code, tools code, signal handlers) but `std::variant` cannot be stored in a `std::atomic`. Phase 13 must resolve this by separating the cross-thread state query (which only checks a few simple conditions like "is quitting?" or "is resizing?") from the full state representation (which lives on the main thread only). The solution is to keep a narrow `atomic<AppState>` "shadow" enum for cross-thread reads while the main thread owns the authoritative `std::variant<state::Running, state::Resizing, ...>` state. Transitions update both: the variant for the main thread's rich state, and the atomic enum for cross-thread queries.

The second challenge is implementing entry/exit actions cleanly. Currently, `process_accumulated_state()` executes multi-step actions (stop runner, recalculate sizes, restart runner) after a state transition. Phase 13 formalizes these as entry actions that fire when a state is entered, and exit actions that fire when a state is left. This replaces the ad-hoc `process_accumulated_state()` function with a structured `transition_to()` function that runs exit action for the old state, constructs the new state, and runs the entry action for the new state.

**Primary recommendation:** Define state structs in `btop_state.hpp` (replacing the enum), use `std::variant` as `AppStateVar` on the main thread, keep the `atomic<AppState>` enum as a cross-thread shadow, and implement `transition_to()` with `std::visit`-based entry/exit action dispatch. The `on_event()` overloads from Phase 12 change their first parameter from `state_tag::X` to `const state::X&` and return the new variant state instead of an enum value.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| STATE-02 | App states carry per-state data via std::variant (Running holds timing, Quitting holds exit code, etc.) | Architecture pattern "State Structs with Per-State Data" enumerates what data each state carries, derived from source code analysis of what `process_accumulated_state()` and the main loop use; Code Examples section shows the complete state struct definitions |
| STATE-03 | Illegal state combinations are unrepresentable at compile time (can't be Running and Quitting simultaneously) | Architecture pattern "Variant as Single-Alternative Container" explains how `std::variant` holds exactly one alternative, making dual-state impossible; the existing `state_tag::` types from Phase 12 merge into the state structs |
| TRANS-03 | Entry/exit actions execute on state transitions (e.g., calcSizes on entering Resizing, timer reset on entering Running) | Architecture pattern "Entry/Exit Actions via transition_to()" documents the entry/exit action table derived from `process_accumulated_state()` and the main loop; Code Examples show the `transition_to()` function implementation |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ `std::variant` | C++17+ (project uses C++23) | Type-safe state union holding exactly one alternative | Already used for `AppEvent`; extending to states is the natural next step |
| C++ `std::visit` | C++17+ | Dispatch on current state for entry/exit actions and event handling | Already used in Phase 12 `dispatch_event()`; the two-variant visit pattern from AUTOMATA-ARCHITECTURE.md section 3.1 becomes directly applicable |
| C++ `std::holds_alternative` | C++17+ | Cross-thread state queries (main thread can check state type) | Lightweight alternative to full visit for simple boolean checks |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Google Test | v1.17.0 | Unit tests for state transitions with data | Already in project; test state data threading and entry/exit action firing |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Shadow atomic enum for cross-thread reads | Mutex-protecting the variant | Mutex adds contention on hot path (runner thread checks state every iteration); shadow enum is zero-cost since it already exists |
| State structs in variant | Keep enum + separate data fields | Defeats the purpose -- data for inactive states would still exist, enabling invalid access |
| Entry/exit action functions | Continue using `process_accumulated_state()` | Less structured, harder to maintain as states grow; entry/exit actions make the FSM self-documenting |

## Architecture Patterns

### The Threading Constraint (Critical)

The current `atomic<AppState>` enum is read from multiple threads:

| Location | Thread | Operation | Purpose |
|----------|--------|-----------|---------|
| `btop.cpp:532` | Runner | `.load() != Quitting` | Runner thread loop guard |
| `btop.cpp:537` | Runner | `.store(Error)` | Runner sets error state |
| `btop.cpp:541` | Runner | `.load() == Resizing` | Runner skips work during resize |
| `btop.cpp:607` | Runner | `.store(Resizing)` | Runner triggers resize on core count change |
| `btop.cpp:713` | Runner | `.store(Error)` | Runner sets error on exception |
| `btop.cpp:847` | Main (Runner::run) | `.load() == Resizing` | Skip output during resize |
| `btop.cpp:897,907` | Main (Runner::stop) | `.load() != Quitting` | Guard in stop logic |
| `btop_draw.cpp:856` | Runner | `.load() != Resizing` | Clock draw guard |
| `btop_draw.cpp:2764` | Runner | `.load() == Resizing` | Proc graph clear guard |
| `btop_tools.cpp:175` | Main | `.compare_exchange_strong()` | Term init clears resize |
| Signal handlers | Signal | via event queue (Phase 11) | Indirect -- signals push events, main loop processes |

**Key insight:** Cross-thread reads only check for two conditions: `== Resizing` and `!= Quitting` (plus `== Quitting` in one place). They never access per-state data. This means the cross-thread interface can remain as a simple atomic enum while the rich variant state lives exclusively on the main thread.

### Dual Representation: Variant + Shadow Enum

```
Main Thread (authoritative):
  AppStateVar state = state::Running{...};   // std::variant with per-state data

Cross-Thread (shadow, read-only query):
  atomic<AppStateTag> state_tag = AppStateTag::Running;   // lightweight enum

Invariant: state_tag always matches the active alternative in state.
Updated together in transition_to() on the main thread only.
```

The existing `Global::app_state` atomic enum is renamed to `Global::state_tag` (or kept as `app_state` for minimal diff). The new `AppStateVar` variant is the authoritative state, owned by the main loop. Every `transition_to()` call updates both.

Runner thread and draw code continue reading `state_tag` (the atomic enum) for their simple boolean checks. They never need the per-state data.

### State Structs with Per-State Data

Each state struct carries data relevant to that state's operation:

```cpp
namespace state {
    struct Running {
        uint64_t update_ms;    // Current update interval
        uint64_t future_time;  // Next timer tick timestamp
    };

    struct Resizing {};  // No data needed -- entry action does the work

    struct Reloading {};  // No data needed -- entry action does the work

    struct Sleeping {};  // No data needed -- entry action suspends process

    struct Quitting {
        int exit_code;  // Exit code for clean_quit()
    };

    struct Error {
        std::string message;  // Error message (replaces Global::exit_error_msg for this state)
    };
}

using AppStateVar = std::variant<
    state::Running,
    state::Resizing,
    state::Reloading,
    state::Sleeping,
    state::Quitting,
    state::Error
>;
```

**Data placement rationale:**

| State | Data | Source (current code) | Why in state struct |
|-------|------|-----------------------|---------------------|
| Running | `update_ms`, `future_time` | Main loop local vars (btop.cpp:1398-1399) | Timer state only meaningful while Running; reset on entry |
| Quitting | `exit_code` | `event::Quit::exit_code` (btop_events.hpp:35) | Exit code determined at quit request; needed by `clean_quit()` |
| Error | `message` | `Global::exit_error_msg` (btop.cpp:117) | Error message set when entering Error state; eliminates global string |
| Resizing | (none) | Entry action calls `Draw::calcSizes()` | All resize work happens in the entry action, no persistent data |
| Reloading | (none) | Entry action calls `init_config()` etc. | All reload work happens in the entry action, no persistent data |
| Sleeping | (none) | Entry action calls `_sleep()` | Process is suspended; no data needed |

**Important:** `state::Error` uses `std::string`, which makes the variant non-trivially-copyable. This is acceptable because:
1. The variant is main-thread-only (not in atomic, not in signal-safe queue)
2. The `AppEvent` variant (for the signal queue) remains trivially copyable and unchanged
3. Error state is entered at most once per process lifetime

### Variant as Single-Alternative Container

`std::variant` holds exactly one alternative at any time. This is the compile-time guarantee that eliminates invalid state combinations:

```cpp
// IMPOSSIBLE with variant:
// state is Running AND Quitting simultaneously
// state is Resizing AND Error simultaneously
// state has exit_code while Running (exit_code only exists in Quitting)

// The compiler enforces this:
AppStateVar state = state::Running{1000, time_ms() + 1000};
// state.exit_code  -- compile error, Running has no exit_code
// To access exit_code, must std::get<state::Quitting>(state) -- throws if not Quitting
```

### Entry/Exit Actions via transition_to()

The current `process_accumulated_state()` function contains the entry actions for each state. Phase 13 formalizes these into a structured `transition_to()` function:

**Entry Actions (derived from current `process_accumulated_state()` and main loop):**

| Target State | Entry Action | Source (current code) |
|--------------|-------------|----------------------|
| Running (from Resizing) | Reset timer: `future_time = time_ms()` | Main loop resets timer implicitly after resize |
| Running (from Sleeping) | (none beyond what `_resume()` does) | `_resume()` is called before state transition |
| Resizing | `term_resize(true); Draw::calcSizes(); Runner::screen_buffer.resize(); Draw::update_clock(true); Menu::process() or Runner::run("all", true, true)` | `process_accumulated_state()` lines 972-980 |
| Reloading | `Runner::stop(); Config::unlock(); init_config(); Theme::updateThemes(); Theme::setTheme(); Draw::update_reset_colors(); Draw::banner_gen()` then chain to Resizing | `process_accumulated_state()` lines 961-971 |
| Sleeping | `_sleep()` | `process_accumulated_state()` line 958 |
| Quitting | `clean_quit(exit_code)` | `process_accumulated_state()` line 954 |
| Error | `clean_quit(1)` (after setting error message) | `process_accumulated_state()` line 950 |

**Exit Actions (derived from current code):**

| Source State | Exit Action | Source (current code) |
|--------------|-------------|----------------------|
| Running | Stop runner if transitioning to Sleeping/Quitting: `Runner::stopping = true` | `on_event(Running, Quit)` and `on_event(Running, Sleep)` |
| Resizing | (none) | Entry action handles everything |
| Reloading | (none) | Entry action handles everything |
| All others | (none significant) | No cleanup needed |

**Implementation pattern:**

```cpp
// Entry action dispatcher
void on_enter(const state::Resizing&, /* context */) {
    term_resize(true);
    Draw::calcSizes();
    Runner::screen_buffer.resize(Term::width, Term::height);
    Draw::update_clock(true);
    if (Menu::active) Menu::process();
    else Runner::run("all", true, true);
    atomic_wait_for(Runner::active, true, 1000);
}

void on_enter(const state::Reloading&, Cli::Cli& cli) {
    if (Runner::active) Runner::stop();
    Config::unlock();
    init_config(cli.low_color, cli.filter);
    Theme::updateThemes();
    Theme::setTheme();
    Draw::update_reset_colors();
    Draw::banner_gen(0, 0, false, true);
    // NOTE: Reloading chains to Resizing -- the on_event handler
    // returns state::Resizing{} after reloading completes
}

void on_enter(const auto&, /* context */) {
    // Default: no entry action
}

// Exit action dispatcher
void on_exit(const state::Running&, const state::Quitting&) {
    if (Runner::active) Runner::stopping = true;
}

void on_exit(const state::Running&, const state::Sleeping&) {
    if (Runner::active) Runner::stopping = true;
}

void on_exit(const auto&, const auto&) {
    // Default: no exit action
}
```

### Transition Flow

The `transition_to()` function replaces both `dispatch_event()` return values and `process_accumulated_state()`:

```
1. on_event(current_state, event) determines the target state
2. If target state differs from current state:
   a. on_exit(current_state, target_state)  -- exit action
   b. Construct new state variant
   c. Update shadow atomic enum
   d. on_enter(new_state, context)  -- entry action
3. Store new variant as current state
```

**The Reloading -> Resizing Chain:** This is handled naturally. When the Reloading entry action completes, the `on_event()` handler for Reloading returns `state::Resizing{}`, which triggers the Resizing entry action. The chain is: `Running -> (exit) -> Reloading -> (enter: reload config) -> Resizing -> (enter: calcSizes) -> Running`.

### on_event() Overload Migration

Phase 12's `on_event()` functions currently take `state_tag::X` as the first parameter and return `AppState` (enum). Phase 13 changes them to take `const state::X&` and return `AppStateVar` (variant):

```cpp
// Phase 12 (current):
static AppState on_event(state_tag::Running, const event::Quit& e, AppState) {
    if (Runner::active) {
        Runner::stopping = true;
        return AppState::Quitting;
    }
    clean_quit(e.exit_code);
    return AppState::Quitting;
}

// Phase 13 (new):
static AppStateVar on_event(const state::Running& s, const event::Quit& e) {
    // Exit action (Runner::stopping) moves to on_exit()
    // Entry action (clean_quit) moves to on_enter()
    return state::Quitting{e.exit_code};
}
```

The side effects (`Runner::stopping`, `clean_quit()`) move from inline in `on_event()` to `on_exit()` and `on_enter()` respectively. The `on_event()` functions become pure state transitions (determine next state), while the actions are in the entry/exit functions.

### dispatch_event() Simplification

With states as variant alternatives instead of enum + tag dispatch, `dispatch_state()` and `state_tag::` namespace are no longer needed. The dispatch becomes a direct two-variant visit:

```cpp
AppStateVar dispatch_event(const AppStateVar& current, const AppEvent& ev) {
    return std::visit(
        [](const auto& state, const auto& event) -> AppStateVar {
            return on_event(state, event);
        },
        current, ev
    );
}
```

This is the canonical `std::visit` over two variants pattern from AUTOMATA-ARCHITECTURE.md section 3.1, now directly applicable because both state and event are variants.

### Recommended File Structure

```
src/
+-- btop_state.hpp      # MODIFIED: Replace enum with state structs + variant typedef
|                        #   Keep AppStateTag enum (renamed from AppState) for cross-thread
|                        #   Add state:: namespace with Running, Resizing, etc.
|                        #   Add AppStateVar = std::variant<state::Running, ...>
|                        #   Add to_tag() helper: AppStateVar -> AppStateTag
+-- btop_events.hpp      # MODIFIED: Remove state_tag:: namespace (merged into state::)
|                        #   Remove dispatch_state() (replaced by direct two-variant visit)
|                        #   Update dispatch_event() signature
+-- btop.cpp             # MODIFIED: on_event() overloads take state:: refs, return AppStateVar
|                        #   Replace process_accumulated_state() with on_enter()/on_exit()
|                        #   Main loop uses AppStateVar, updates shadow enum
|                        #   Entry/exit action functions
+-- btop_draw.cpp        # MINOR: Update AppState -> AppStateTag in two reads
+-- btop_tools.cpp       # MINOR: Update AppState -> AppStateTag in one CAS
+-- btop_shared.hpp      # MINOR: Update extern declaration
```

### Anti-Patterns to Avoid

- **Removing the atomic enum entirely:** The runner thread, draw code, and tools code read `atomic<AppState>` from non-main threads. Removing it requires mutex-protecting the variant, which adds contention on a hot path. Keep the atomic enum as a shadow.
- **Making the variant atomic:** `std::atomic<std::variant<...>>` does not work for non-trivially-copyable types (and even for trivially copyable ones, it would be a 40+ byte atomic which is not lock-free on any architecture).
- **Putting entry/exit actions inside the state structs:** State structs should be plain data. Actions depend on context (Runner, Config, Draw) that the state structs shouldn't know about.
- **Making on_event() return a raw state struct instead of the variant:** The return type must be `AppStateVar` (the variant) so that transitions can change the active alternative. Returning a raw struct requires the caller to wrap it.
- **Over-designing the entry/exit action system:** Use simple function overloads, not a registration/callback system. The state set is fixed and small (6 states).
- **Moving timer data (update_ms, future_time) into the variant prematurely:** These values need to survive across Running -> Resizing -> Running transitions. Consider whether they belong in state::Running (reset on entry) or remain as main loop locals (preserved across transitions). The current code resets the timer after resize, so state::Running carrying timer data with reset-on-entry is correct.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| State-specific data access | `switch` on enum + separate data map | `std::get<state::T>(variant)` or `std::visit` | Compiler-enforced access only when in correct state |
| Entry/exit action dispatch | Manual if/else chains | Overloaded `on_enter()`/`on_exit()` with `std::visit` | Type-safe, exhaustive, extensible |
| Cross-thread state synchronization | Mutex around variant | Shadow atomic enum + main-thread-only variant | Lock-free reads, zero contention |
| State type to enum conversion | Manual switch mapping | `to_tag()` helper using `std::visit` + index | Single source of truth, can't get out of sync |

**Key insight:** The transition from enum to variant is primarily a main-thread concern. The cross-thread interface barely changes -- it still reads an atomic enum. The complexity is in restructuring the main loop to use variant state and in formalizing the entry/exit actions that currently live in `process_accumulated_state()`.

## Common Pitfalls

### Pitfall 1: Breaking Cross-Thread State Queries
**What goes wrong:** Removing `atomic<AppState>` breaks runner thread loop guard (`!= Quitting`), draw code resize checks (`!= Resizing`), and tools code CAS (`compare_exchange_strong`).
**Why it happens:** The variant cannot be atomically loaded from other threads. Forgetting to maintain the shadow enum leaves cross-thread reads with no valid state to check.
**How to avoid:** Keep the atomic enum as a shadow. Rename it to `AppStateTag` (or keep the name). Every `transition_to()` call updates both the variant and the shadow enum. Cross-thread code continues reading the atomic.
**Warning signs:** Compiler errors in btop_draw.cpp, btop_tools.cpp. Runner thread hangs or doesn't stop on quit.

### Pitfall 2: Reloading -> Resizing Chain Broken
**What goes wrong:** The Reloading entry action runs, but the chain to Resizing (which triggers `calcSizes()`) doesn't fire because the transition_to() only runs one entry action.
**Why it happens:** `process_accumulated_state()` currently chains: if Reloading, do reload work, then set state to Resizing, then fall through to the Resizing handler. With entry/exit actions, the Reloading entry action completes but `on_enter(Resizing)` doesn't automatically fire.
**How to avoid:** The `on_event()` handler for Reloading events should return `state::Resizing{}` (not `state::Running{}`). After the Reloading entry action runs, `transition_to()` is called again with the Resizing target. Alternatively, the Reloading entry action can explicitly trigger a transition to Resizing. The cleanest approach: make the Reloading entry action perform the reload, then have the transition function return `state::Resizing{}`, which triggers the Resizing entry action.
**Warning signs:** Config reload (Ctrl+R / SIGUSR2) completes but screen doesn't refresh.

### Pitfall 3: Error Message Lifetime
**What goes wrong:** `state::Error{message}` stores the error message in the variant, but `clean_quit()` (called in the Error entry action) reads `Global::exit_error_msg`. If the message is only in the variant, `clean_quit()` can't find it.
**Why it happens:** `clean_quit()` is also called from places outside the FSM (runner thread, signal handlers, init code) where there is no variant state.
**How to avoid:** During Phase 13, keep `Global::exit_error_msg` AND store the message in `state::Error`. The entry action for Error copies the message to `Global::exit_error_msg` before calling `clean_quit()`. This is a bridge pattern -- the global can be removed when all callers go through the FSM (Phase 14+).
**Warning signs:** Empty error messages when btop crashes.

### Pitfall 4: Timer State Lost Across Transitions
**What goes wrong:** `state::Running` carries `update_ms` and `future_time`. When transitioning Running -> Resizing -> Running, the timer values are reconstructed with defaults instead of appropriate values.
**Why it happens:** Each entry into Running creates a new `state::Running{}` struct. If the constructor doesn't set appropriate timer values, the first update happens immediately or at the wrong interval.
**How to avoid:** The `on_enter(state::Running&)` action (or the constructor) should set `future_time = time_ms()` (trigger immediate collection after resize, matching current behavior) and `update_ms = Config::getI(IntKey::update_ms)` (read current config).
**Warning signs:** Data collection happens immediately after every state transition, or stops collecting entirely.

### Pitfall 5: Variant Size and Alignment
**What goes wrong:** `state::Error` contains a `std::string` (typically 32 bytes on most implementations). The variant is as large as its largest alternative plus discriminator overhead. If placed in a cache-sensitive location, this can affect performance.
**Why it happens:** `std::variant` always allocates space for the largest alternative.
**How to avoid:** The variant is a main-thread local variable, not in a hot loop data structure. Size is not a concern. However, verify that the variant does not accidentally end up in a cache line shared with atomic variables read by other threads (false sharing).
**Warning signs:** None expected -- this is a theoretical concern, not a practical one for a single main-loop variable.

## Code Examples

### State Structs (btop_state.hpp)

```cpp
// src/btop_state.hpp — Application lifecycle state definitions
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace Global {

    /// Lightweight tag enum for cross-thread state queries.
    /// The runner thread and draw code read this atomically.
    /// The main thread updates this whenever the variant state changes.
    enum class AppStateTag : std::uint8_t {
        Running   = 0,
        Resizing  = 1,
        Reloading = 2,
        Sleeping  = 3,
        Quitting  = 4,
        Error     = 5,
    };

    extern std::atomic<AppStateTag> app_state;  // shadow enum for cross-thread reads

    constexpr std::string_view to_string(AppStateTag s) noexcept {
        switch (s) {
            case AppStateTag::Running:   return "Running";
            case AppStateTag::Resizing:  return "Resizing";
            case AppStateTag::Reloading: return "Reloading";
            case AppStateTag::Sleeping:  return "Sleeping";
            case AppStateTag::Quitting:  return "Quitting";
            case AppStateTag::Error:     return "Error";
        }
        return "Unknown";
    }

} // namespace Global

/// Typed state structs carrying per-state data.
namespace state {
    struct Running {
        uint64_t update_ms;    // Current update interval from config
        uint64_t future_time;  // Timestamp of next timer tick
    };

    struct Resizing {};

    struct Reloading {};

    struct Sleeping {};

    struct Quitting {
        int exit_code;
    };

    struct Error {
        std::string message;
    };
}

/// The authoritative application state. Main-thread only.
using AppStateVar = std::variant<
    state::Running,
    state::Resizing,
    state::Reloading,
    state::Sleeping,
    state::Quitting,
    state::Error
>;

/// Convert variant state to tag enum for shadow updates.
inline Global::AppStateTag to_tag(const AppStateVar& s) noexcept {
    using Global::AppStateTag;
    return std::visit([](const auto& st) -> AppStateTag {
        using T = std::decay_t<decltype(st)>;
        if constexpr (std::is_same_v<T, state::Running>)   return AppStateTag::Running;
        if constexpr (std::is_same_v<T, state::Resizing>)  return AppStateTag::Resizing;
        if constexpr (std::is_same_v<T, state::Reloading>) return AppStateTag::Reloading;
        if constexpr (std::is_same_v<T, state::Sleeping>)  return AppStateTag::Sleeping;
        if constexpr (std::is_same_v<T, state::Quitting>)  return AppStateTag::Quitting;
        if constexpr (std::is_same_v<T, state::Error>)     return AppStateTag::Error;
    }, s);
}
```

### Entry/Exit Actions (btop.cpp)

```cpp
// --- Exit actions (called when LEAVING a state) ---

static void on_exit(const state::Running&, const state::Quitting&) {
    if (Runner::active) Runner::stopping = true;
}

static void on_exit(const state::Running&, const state::Sleeping&) {
    if (Runner::active) Runner::stopping = true;
}

static void on_exit(const auto&, const auto&) {
    // Default: no exit action
}

// --- Entry actions (called when ENTERING a state) ---

static void on_enter(state::Resizing&) {
    term_resize(true);
    Draw::calcSizes();
    Runner::screen_buffer.resize(Term::width, Term::height);
    Draw::update_clock(true);
    if (Menu::active) Menu::process();
    else Runner::run("all", true, true);
    atomic_wait_for(Runner::active, true, 1000);
}

static void on_enter(state::Reloading&, Cli::Cli& cli) {
    if (Runner::active) Runner::stop();
    Config::unlock();
    init_config(cli.low_color, cli.filter);
    Theme::updateThemes();
    Theme::setTheme();
    Draw::update_reset_colors();
    Draw::banner_gen(0, 0, false, true);
    // After reload, transition chains to Resizing (handled by caller)
}

static void on_enter(state::Sleeping&) {
    _sleep();
}

static void on_enter(state::Quitting& q) {
    clean_quit(q.exit_code);
}

static void on_enter(state::Error& e) {
    Global::exit_error_msg = std::move(e.message);
    clean_quit(1);
}

static void on_enter(state::Running&) {
    // Timer reset happens via state data initialization
}
```

### Transition Function (btop.cpp)

```cpp
/// Execute a state transition with entry/exit actions.
/// Updates both the variant state and the shadow atomic enum.
static void transition_to(AppStateVar& current, AppStateVar next, /* context */) {
    if (to_tag(current) == to_tag(next)) {
        // Same state type -- update data in place, no entry/exit actions
        current = std::move(next);
        return;
    }

    // Exit action: dispatch on (old_state, new_state)
    std::visit([&](const auto& old_st, const auto& new_st) {
        on_exit(old_st, new_st);
    }, current, next);

    // Update shadow enum for cross-thread readers
    Global::app_state.store(to_tag(next), std::memory_order_release);

    // Move new state into current
    current = std::move(next);

    // Entry action: dispatch on new state
    std::visit([&](auto& new_st) {
        on_enter(new_st, /* context */);
    }, current);
}
```

### Updated on_event() Overloads (btop.cpp)

```cpp
// Phase 13: on_event takes state refs, returns variant

static AppStateVar on_event(const state::Running& s, const event::Quit& e) {
    return state::Quitting{e.exit_code};
    // Runner::stopping moved to on_exit(Running, Quitting)
}

static AppStateVar on_event(const state::Running&, const event::Sleep&) {
    return state::Sleeping{};
    // Runner::stopping moved to on_exit(Running, Sleeping)
}

static AppStateVar on_event(const state::Running&, const event::Resume&) {
    return state::Running{/* timer values */};  // resume handled before dispatch
}

static AppStateVar on_event(const state::Running&, const event::Resize&) {
    return state::Resizing{};
}

static AppStateVar on_event(const state::Running&, const event::Reload&) {
    return state::Reloading{};
}

static AppStateVar on_event(const state::Running& s, const event::TimerTick&) {
    return s;  // Running stays Running; timer action in main loop
}

static AppStateVar on_event(const state::Running& s, const event::KeyInput&) {
    return s;  // Running stays Running; input action in main loop
}

static AppStateVar on_event(const state::Running&, const event::ThreadError&) {
    return state::Error{"Thread error"};
}

// Catch-all: unhandled (state, event) pairs preserve current state
static AppStateVar on_event(const auto& s, const auto&) {
    return s;  // Return copy of current state
}
```

### Updated dispatch_event() (btop_events.hpp or btop.cpp)

```cpp
// Two-variant visit -- no more dispatch_state() or state_tag:: needed
AppStateVar dispatch_event(const AppStateVar& current, const AppEvent& ev) {
    return std::visit(
        [](const auto& state, const auto& event) -> AppStateVar {
            return on_event(state, event);
        },
        current, ev
    );
}
```

### Updated Main Loop (btop.cpp)

```cpp
// Main-thread-only variant state
AppStateVar app_var = state::Running{
    static_cast<uint64_t>(Config::getI(IntKey::update_ms)),
    time_ms()
};

while (not true not_eq not false) {
    // 1. Drain signal events
    while (auto ev = Global::event_queue.try_pop()) {
        if (std::holds_alternative<event::Resume>(*ev)) {
            _resume();
        }
        auto next = dispatch_event(app_var, *ev);
        transition_to(app_var, std::move(next));
        auto tag = to_tag(app_var);
        if (tag == AppStateTag::Quitting or tag == AppStateTag::Error) break;
    }

    // 2. Reloading chains to Resizing
    if (std::holds_alternative<state::Reloading>(app_var)) {
        transition_to(app_var, state::Resizing{});
    }

    // 3. Resizing returns to Running
    if (std::holds_alternative<state::Resizing>(app_var)) {
        transition_to(app_var, state::Running{
            static_cast<uint64_t>(Config::getI(IntKey::update_ms)),
            time_ms()
        });
    }

    // 4. Sleeping returns to Running
    if (std::holds_alternative<state::Sleeping>(app_var)) {
        transition_to(app_var, state::Running{
            static_cast<uint64_t>(Config::getI(IntKey::update_ms)),
            time_ms()
        });
    }

    // ... rest of main loop uses std::get<state::Running>(app_var)
    // for timer values instead of local variables
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Enum state + separate data | Variant states with per-state data | C++17 std::variant | State-specific data only accessible in correct state |
| Implicit entry/exit actions in if/else chains | Typed on_enter/on_exit overloads | General FSM pattern | Self-documenting, testable transition actions |
| Single atomic for all threads | Shadow enum + main-thread variant | Phase 13 pattern | Rich state on main thread, lightweight cross-thread queries |
| `process_accumulated_state()` function | Entry/exit action dispatch | Phase 13 | Actions are colocated with state definitions, not in a monolithic function |

## Open Questions

1. **Should `state::Running` carry timer data or should timer data remain as main loop locals?**
   - What we know: `update_ms` and `future_time` are currently local variables in the main loop. They are reset after resize (implicitly, by the timer logic). Putting them in `state::Running` makes the reset explicit (new `state::Running{}` gets fresh values) and eliminates two local variables.
   - What's unclear: Whether future phases (Runner FSM in Phase 14) will need to access timer data, which would complicate data placement.
   - Recommendation: Put them in `state::Running`. The reset-on-entry semantic is cleaner and more FSM-idiomatic. If Phase 14 needs timer data, it can be passed explicitly.

2. **Should the `AppState` enum be renamed to `AppStateTag` or kept as `AppState`?**
   - What we know: Renaming to `AppStateTag` is clearer (distinguishes the tag enum from the variant type) but creates a larger diff across btop_draw.cpp, btop_tools.cpp, and the runner thread code.
   - What's unclear: Whether the larger diff is justified for clarity.
   - Recommendation: Rename to `AppStateTag`. The diff is mechanical (find/replace) and the clarity is worth it. Cross-thread code should clearly use the "tag" (lightweight enum), not the "state" (rich variant).

3. **How should the Reloading -> Resizing chain work with entry/exit actions?**
   - What we know: Currently, `process_accumulated_state()` handles Reloading (does reload work), then sets state to Resizing, then falls through to the Resizing handler. With entry/exit actions, this needs two transitions.
   - What's unclear: Whether to chain the transitions inside `on_enter(Reloading)` (entry action triggers another transition) or in the main loop (check for Reloading state after entry action, then transition to Resizing).
   - Recommendation: Handle in the main loop. After draining events and running transition actions, check if state is Reloading (entry action completed). If so, transition to Resizing. Then check if Resizing (entry action completed), transition to Running. This matches the current flow and keeps entry actions simple (no nested transitions).

4. **Should `Global::exit_error_msg` be eliminated in Phase 13?**
   - What we know: `state::Error` carries the message. But `clean_quit()` and runner thread code still set `Global::exit_error_msg` directly.
   - What's unclear: Whether eliminating the global is feasible without also refactoring `clean_quit()` and runner thread error handling.
   - Recommendation: Keep `Global::exit_error_msg` in Phase 13 as a bridge. The Error entry action copies the message from `state::Error` to the global. Full elimination happens when runner thread goes through the FSM (Phase 14).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 (via FetchContent) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R "State\|Transition" --output-on-failure` |
| Full suite command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| STATE-02 | state::Running carries update_ms and future_time | unit | `ctest --test-dir build/tests -R StateData --output-on-failure` | Wave 0 |
| STATE-02 | state::Quitting carries exit_code | unit | `ctest --test-dir build/tests -R StateData --output-on-failure` | Wave 0 |
| STATE-02 | state::Error carries message string | unit | `ctest --test-dir build/tests -R StateData --output-on-failure` | Wave 0 |
| STATE-03 | AppStateVar holds exactly one alternative (variant) | unit | `ctest --test-dir build/tests -R StateVariant --output-on-failure` | Wave 0 |
| STATE-03 | Cannot access Quitting::exit_code while in Running (compile-time) | compile-time (static_assert or SFINAE test) | Build succeeds | Wave 0 |
| STATE-03 | to_tag() maps each variant alternative to correct enum value | unit | `ctest --test-dir build/tests -R StateTag --output-on-failure` | Wave 0 |
| TRANS-03 | on_event(Running, Resize) returns state::Resizing | unit | `ctest --test-dir build/tests -R TypedTransition --output-on-failure` | Wave 0 |
| TRANS-03 | on_event(Running, Quit) returns state::Quitting with exit_code | unit | `ctest --test-dir build/tests -R TypedTransition --output-on-failure` | Wave 0 |
| TRANS-03 | on_event(Running, ThreadError) returns state::Error with message | unit | `ctest --test-dir build/tests -R TypedTransition --output-on-failure` | Wave 0 |
| TRANS-03 | dispatch_event two-variant visit routes correctly | unit | `ctest --test-dir build/tests -R TypedDispatch --output-on-failure` | Wave 0 |
| TRANS-03 | Entry action fires on state change (verifiable via mock/flag) | unit | `ctest --test-dir build/tests -R EntryExit --output-on-failure` | Wave 0 |
| TRANS-03 | Exit action fires on state change (verifiable via mock/flag) | unit | `ctest --test-dir build/tests -R EntryExit --output-on-failure` | Wave 0 |
| TRANS-03 | No entry/exit action when state type unchanged (self-transition) | unit | `ctest --test-dir build/tests -R EntryExit --output-on-failure` | Wave 0 |
| ALL | Full application runs identically after Phase 13 changes | integration (manual) | Build + btop runs with resize, quit, sleep, reload | manual |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure`
- **Per wave merge:** Full suite + manual build and test on macOS
- **Phase gate:** Full suite green + manual verification: resize, Ctrl+C, Ctrl+Z/fg, kill -USR2, key input, menu navigation, thread error handling

### Wave 0 Gaps
- [ ] `tests/test_app_state.cpp` -- UPDATE existing tests: AppState -> AppStateTag rename, add state struct data tests
- [ ] `tests/test_transitions.cpp` -- UPDATE existing tests: change from enum returns to variant returns, test state data in transitions
- [ ] `tests/test_events.cpp` -- UPDATE: remove state_tag:: tests (merged into state::), update dispatch_event tests for two-variant visit
- [ ] New test functions for: to_tag() mapping, variant single-alternative guarantee, entry/exit action firing

**Important test design constraint:** Entry/exit action tests cannot call real `clean_quit()`, `Draw::calcSizes()`, etc. (they depend on btop.cpp globals). Tests should verify:
1. State data is correctly constructed and accessible
2. `to_tag()` maps every alternative correctly
3. `dispatch_event()` routes correctly with two-variant visit
4. Transition functions return correct variant types with correct data
5. Entry/exit action overload resolution works (compile-time check)

Guard-dependent behavior (Runner::active checks in exit actions) requires either mock injection or is verified manually and in Phase 15.

## Sources

### Primary (HIGH confidence)
- Source code analysis of `src/btop_state.hpp` (current AppState enum, Phase 10)
- Source code analysis of `src/btop_events.hpp` (AppEvent variant, state_tag namespace, dispatch_state, Phase 11-12)
- Source code analysis of `src/btop.cpp` lines 295-354 (on_event overloads, dispatch_event, Phase 12)
- Source code analysis of `src/btop.cpp` lines 948-983 (process_accumulated_state -- entry actions source)
- Source code analysis of `src/btop.cpp` lines 1393-1464 (main loop -- timer/input handling)
- Source code analysis of `src/btop.cpp` lines 520-545 (runner thread -- cross-thread state reads)
- Source code analysis of `src/btop.cpp` lines 88-127 (Global namespace definitions)
- Source code analysis of `src/btop_draw.cpp` lines 856, 2764 (cross-thread state reads)
- Source code analysis of `src/btop_tools.cpp` line 175 (cross-thread CAS)
- Source code analysis of `src/btop_shared.hpp` lines 399-421 (extern declarations, runner interface)
- `.planning/research/AUTOMATA-ARCHITECTURE.md` section 3.1 (variant+visit pattern, two-variant dispatch)
- `.planning/phases/12-extract-transitions/12-RESEARCH.md` (Phase 12 architecture, on_event patterns)
- `.planning/phases/10-name-states/10-RESEARCH.md` (Phase 10 flag taxonomy, threading analysis)
- `.planning/REQUIREMENTS.md` (STATE-02, STATE-03, TRANS-03 definitions)
- `.planning/STATE.md` (accumulated decisions from Phases 10-12)
- `tests/test_app_state.cpp`, `tests/test_transitions.cpp`, `tests/test_events.cpp` (existing test infrastructure)

### Secondary (MEDIUM confidence)
- [cppreference: std::variant](https://en.cppreference.com/w/cpp/utility/variant) - Variant semantics, single-alternative guarantee, std::visit with multiple variants
- [cppreference: std::visit](https://en.cppreference.com/w/cpp/utility/variant/visit) - Multi-variant visit (two-variant dispatch pattern)
- C++ standard: `std::variant` is NOT lock-free atomically storable -- confirmed by standard (no `std::atomic<std::variant>` specialization exists for non-trivially-copyable types)

### Tertiary (LOW confidence)
- None. All findings are from direct source code analysis and C++ standard documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Using only std::variant and std::visit already present in the codebase (AppEvent is already a variant); no new libraries
- Architecture: HIGH - The dual representation (variant + shadow enum) pattern is directly derived from analyzing all cross-thread read sites in the source code; the entry/exit action table is derived from the existing process_accumulated_state() function
- Pitfalls: HIGH - All pitfalls identified from actual cross-thread access analysis (runner reads, draw reads, tools CAS) and from the Reloading->Resizing chain in process_accumulated_state()

**Research date:** 2026-02-28
**Valid until:** 2026-03-28 (stable -- no external dependencies, pure internal refactoring using C++ standard library)
