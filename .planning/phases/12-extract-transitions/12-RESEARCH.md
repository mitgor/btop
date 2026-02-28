# Phase 12: Extract Transitions - Research

**Researched:** 2026-02-28
**Domain:** Typed state transition dispatch using std::visit over (state, event) pairs in C++23
**Confidence:** HIGH

## Summary

Phase 12 transforms btop's main loop from a two-step pattern (drain events into atomic state, then check atomic state via if/else if chain) into a single-step event-driven dispatch using `std::visit`. Currently (after Phases 10-11), signal handlers push typed events into a lock-free queue, and the main loop drains them into `process_signal_event()` overloads that write to `atomic<AppState>`, which is then read by an if/else if chain that executes the corresponding actions. Phase 12 eliminates this indirection: the main loop will `std::visit` over `(AppState, AppEvent)` pairs, dispatching directly to named `on_event()` transition functions that both determine the next state and execute the associated actions.

The key challenge is that Phase 12 must also bring **input events** (key presses, timer ticks) into the event-driven model. Currently, input is handled separately via `Input::poll()` -> `Input::get()` -> `Input::process()` at the bottom of the main loop, and timer expiry triggers `Runner::run()` directly. Phase 12 wraps these into the same event variant (`event::TimerTick`, `event::KeyInput`) so that ALL main-loop stimuli flow through the unified dispatch. The `event::TimerTick` struct already exists in the variant (added in Phase 11 for forward compatibility); `event::KeyInput` needs to be added.

The second challenge is preserving the existing priority semantics. The current if/else if chain enforces `Error > Quitting > Sleeping > Reloading > Resizing > Running` by checking states in that order. With event-driven dispatch, priority is preserved differently: terminal states (Error, Quitting) cause the dispatch loop to stop processing further events (early exit), and the transition functions themselves enforce that higher-priority states cannot be overridden by lower-priority events (e.g., a Resize event while Quitting does nothing).

**Primary recommendation:** Add `event::KeyInput` to the `AppEvent` variant (with a small fixed-size key buffer, not `std::string`, to maintain trivial copyability). Replace the `process_signal_event()` overloads and the if/else if chain with `on_event(AppState, Event)` overload sets dispatched via `std::visit`. Keep `AppState` as the enum (Phase 13 graduates it to `std::variant`). The main loop becomes: drain queue + generate timer/input events -> for each event, `std::visit` to `on_event()` -> execute actions and update state.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TRANS-01 | Transition functions extracted from if/else if chain into typed on_event() overloads | Architecture pattern "Typed Transition Functions" shows how each state+event combination maps to a named `on_event()` overload; the full transition table is enumerated in "Code Examples > Complete Transition Table" |
| TRANS-02 | std::visit dispatches (state, event) pairs to correct transition function | Architecture pattern "Double Dispatch via std::visit" demonstrates the two-variant visit pattern with AppState enum converted to a tag type for dispatch; "Code Examples > Main Loop Dispatch" shows the complete implementation |
| TRANS-04 | All existing state transition semantics preserved (priority ordering, guard conditions) | "Common Pitfalls > Pitfall 1: Priority Inversion" documents how priority is preserved through early-exit on terminal states and guard conditions in transition functions; "Architecture Patterns > Priority Preservation Strategy" maps each priority level to its enforcement mechanism |
| EVENT-04 | Main loop consumes events from queue and dispatches to transition functions | Architecture pattern "Event-Driven Main Loop" shows the unified event loop that drains signal events, generates timer/input events, and dispatches all through `on_event()` overloads |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ `std::visit` | C++17+ (project uses C++23) | Double dispatch over (state_tag, event) pairs | Already used in Phase 11 event drain; zero-cost jump-table dispatch |
| C++ `std::variant` | C++17+ | Type-safe event union (AppEvent extended with KeyInput) | Already in use for events; extending with one more alternative |
| C++ `std::overloaded` pattern | C++17+ | Lambda-based visitor construction | Standard idiom for `std::visit`; avoids separate visitor class |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Google Test | v1.17.0 | Unit tests for transition functions | Already in project; test each (state, event) -> next_state mapping |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Free function `on_event()` overloads | Visitor class with `operator()` overloads | Visitor class groups all transitions in one place but is harder to read with many combinations; free functions allow grouping by state or by event |
| `std::visit` on (state_tag, event) | Manual switch on AppState + `std::visit` on event only | Loses compile-time exhaustiveness for state dimension; manual switch can miss states |
| Adding KeyInput to AppEvent variant | Keep input handling separate from event dispatch | Leaves input as a special case outside the FSM; Phase 13 would have to integrate it anyway |

## Architecture Patterns

### The Dispatch Problem: Enum State + Variant Event

Phase 12 has a unique constraint: `AppState` is still an enum (not a variant until Phase 13), but events are a `std::variant`. This means we cannot do a direct two-variant `std::visit(visitor, state_variant, event_variant)` as the domain research envisioned. Instead, we use a hybrid approach:

**Option A (Recommended): State tag types for dispatch**

Convert the AppState enum to a compile-time tag for dispatch purposes:

```cpp
namespace state_tag {
    struct Running {};
    struct Resizing {};
    struct Reloading {};
    struct Sleeping {};
    struct Quitting {};
    struct Error {};
}

// Convert runtime enum to compile-time tag via std::visit-like dispatch
template<typename Visitor>
auto dispatch_state(AppState s, Visitor&& vis) {
    switch (s) {
        case AppState::Running:   return vis(state_tag::Running{});
        case AppState::Resizing:  return vis(state_tag::Resizing{});
        case AppState::Reloading: return vis(state_tag::Reloading{});
        case AppState::Sleeping:  return vis(state_tag::Sleeping{});
        case AppState::Quitting:  return vis(state_tag::Quitting{});
        case AppState::Error:     return vis(state_tag::Error{});
    }
    __builtin_unreachable();
}
```

Then the main dispatch becomes:

```cpp
AppState dispatch_event(AppState current, const AppEvent& ev) {
    return dispatch_state(current, [&](auto state_tag) {
        return std::visit([&](const auto& event) {
            return on_event(state_tag, event);
        }, ev);
    });
}
```

This gives us full (state, event) dispatch with named overloads for each combination.

**Option B: Single visit on event, switch on state inside each handler**

Simpler but loses compile-time exhaustiveness for the state dimension:

```cpp
AppState handle_event(AppState s, const AppEvent& ev) {
    return std::visit([s](const auto& e) -> AppState {
        return on_event(s, e);  // overloaded on event type, switches on s inside
    }, ev);
}
```

**Recommendation: Option A.** It provides compile-time exhaustiveness for both dimensions. When Phase 13 replaces AppState enum with `std::variant`, the `dispatch_state()` function simply becomes another `std::visit`, and all `on_event()` overloads remain unchanged.

### KeyInput Event Design

The current `event::TimerTick` and `event::ThreadError` structs are already in the AppEvent variant. Phase 12 needs to add `event::KeyInput` for input events. The challenge: key strings can be up to ~20 characters (mouse events like `[<0;123;456M`), but `std::string` is not trivially copyable.

**Solution: Fixed-size key buffer**

```cpp
namespace event {
    struct KeyInput {
        std::array<char, 32> key_data{};
        std::uint8_t key_len{0};

        // Convenience constructor from string_view
        explicit KeyInput(std::string_view k) noexcept
            : key_len(static_cast<std::uint8_t>(std::min(k.size(), size_t{31}))) {
            std::copy_n(k.data(), key_len, key_data.data());
        }

        std::string_view key() const noexcept {
            return {key_data.data(), key_len};
        }
    };
}
```

This keeps AppEvent trivially copyable (required for the signal-safe queue) while supporting all key input strings. The 32-byte buffer is sufficient for any key sequence btop handles.

**Important:** KeyInput events are NOT pushed from signal handlers. They are generated by the main loop after `Input::poll()` returns true. The trivially-copyable constraint is maintained for the variant as a whole, but KeyInput specifically does not require signal safety.

### Typed Transition Functions

Each `(state, event)` pair gets a named `on_event()` overload. Transition functions:
1. Receive the current state tag and the event
2. Execute any side effects (actions)
3. Return the new AppState enum value

```cpp
// --- Running state transitions ---
AppState on_event(state_tag::Running, const event::Quit& e) {
    if (Runner::active) {
        Runner::stopping = true;
        return AppState::Quitting;
    }
    clean_quit(e.exit_code);
    return AppState::Quitting;  // unreachable, clean_quit exits
}

AppState on_event(state_tag::Running, const event::Sleep&) {
    if (Runner::active) {
        Runner::stopping = true;
        return AppState::Sleeping;
    }
    _sleep();
    return AppState::Running;  // after resume
}

AppState on_event(state_tag::Running, const event::Resume&) {
    _resume();
    return AppState::Running;
}

AppState on_event(state_tag::Running, const event::Resize&) {
    term_resize();
    return AppState::Resizing;
}

AppState on_event(state_tag::Running, const event::Reload&) {
    return AppState::Reloading;
}

AppState on_event(state_tag::Running, const event::TimerTick&) {
    Runner::run("all");
    return AppState::Running;
}

AppState on_event(state_tag::Running, const event::KeyInput& e) {
    if (Menu::active) {
        Menu::process(e.key());
    } else {
        Input::process(e.key());
    }
    return AppState::Running;
}

AppState on_event(state_tag::Running, const event::ThreadError&) {
    return AppState::Error;
}

// --- Catch-all: unhandled (state, event) pairs are no-ops ---
AppState on_event(const auto&, const auto&, AppState current) {
    return current;  // no transition
}
```

### Priority Preservation Strategy

The existing priority chain is:

```
Error > Quitting > Sleeping > Reloading > Resizing > TimerTick > KeyInput
```

With event-driven dispatch, priority is maintained through three mechanisms:

1. **Early exit on terminal states:** After processing each event, check if the new state is Error or Quitting. If so, stop processing further events from the queue:

```cpp
while (auto ev = Global::event_queue.try_pop()) {
    new_state = dispatch_event(current_state, *ev);
    current_state = new_state;
    if (new_state == AppState::Quitting or new_state == AppState::Error) break;
}
```

2. **Guard conditions in transition functions:** A Resize event when already Quitting is a no-op (the catch-all returns current state). A Reload event when already Sleeping is a no-op. The transition functions enforce this via guards:

```cpp
// Quitting state ignores everything except ThreadError (escalates to Error)
AppState on_event(state_tag::Quitting, const event::ThreadError&) {
    return AppState::Error;
}
// All other events in Quitting are no-ops (via catch-all)
```

3. **Processing order within a single iteration:** Signal events (from queue) are processed first, then timer logic, then input. This matches the existing flow.

### Event-Driven Main Loop

The restructured main loop:

```cpp
while (true) {
    auto state = Global::app_state.load(std::memory_order_acquire);

    // 1. Drain signal events from queue
    while (auto ev = Global::event_queue.try_pop()) {
        state = dispatch_event(state, *ev);
        Global::app_state.store(state);
        if (state == AppState::Quitting or state == AppState::Error) break;
    }

    // 2. Handle accumulated state (Reloading/Resizing have multi-step actions)
    state = process_accumulated_state(state);

    // 3. Check timer expiry -> generate TimerTick event
    if (time_ms() >= future_time and state == AppState::Running) {
        state = dispatch_event(state, event::TimerTick{});
        Global::app_state.store(state);
        update_ms = Config::getI(IntKey::update_ms);
        future_time = time_ms() + update_ms;
    }

    // 4. Poll for input -> generate KeyInput events
    for (auto current_time = time_ms(); current_time < future_time; current_time = time_ms()) {
        if (Input::poll(min(1000ULL, future_time - current_time))) {
            if (not Runner::active) Config::unlock();
            auto key = Input::get();
            if (not key.empty()) {
                state = dispatch_event(state, event::KeyInput{key});
                Global::app_state.store(state);
            }
        } else {
            break;
        }
    }
}
```

**Key insight:** The Reloading and Resizing states involve multi-step actions (stop runner, reinit config, recalculate sizes, trigger redraw) that don't fit neatly into a single transition function. These are handled by a `process_accumulated_state()` function that executes the multi-step sequence and transitions back to Running. This mirrors the current if/else if body for these states.

### Handling Reloading and Resizing States

These two states have complex multi-step behavior that occurs after the state is set but before the next event is processed:

**Reloading** (current code, lines 1359-1368):
```cpp
if (Runner::active) Runner::stop();
Config::unlock();
init_config(cli.low_color, cli.filter);
Theme::updateThemes();
Theme::setTheme();
Draw::update_reset_colors();
Draw::banner_gen(0, 0, false, true);
Global::app_state.store(AppState::Resizing);  // chains to Resizing
```

**Resizing** (current code, lines 1371-1383):
```cpp
term_resize(state == AppState::Resizing);
if (state == AppState::Resizing or Global::app_state.load() == AppState::Resizing) {
    Draw::calcSizes();
    Runner::screen_buffer.resize(Term::width, Term::height);
    Draw::update_clock(true);
    auto expected = AppState::Resizing;
    Global::app_state.compare_exchange_strong(expected, AppState::Running);
    if (Menu::active) Menu::process();
    else Runner::run("all", true, true);
    atomic_wait_for(Runner::active, true, 1000);
}
```

**Approach:** These multi-step sequences remain as dedicated functions called after the event drain loop. They are NOT individual event transitions -- they are "state entry actions" (which Phase 13 will formalize). For Phase 12:

```cpp
AppState process_accumulated_state(AppState state) {
    if (state == AppState::Reloading) {
        // ... multi-step reload sequence ...
        return AppState::Resizing;  // chains to resize
    }
    if (state == AppState::Resizing) {
        // ... multi-step resize sequence ...
        return AppState::Running;
    }
    if (state == AppState::Sleeping) {
        _sleep();
        return AppState::Running;
    }
    if (state == AppState::Error) {
        clean_quit(1);
    }
    if (state == AppState::Quitting) {
        clean_quit(0);
    }
    return state;
}
```

### Recommended File Structure

```
src/
+-- btop_events.hpp     # MODIFIED: Add event::KeyInput, add state_tag namespace, add dispatch helpers
+-- btop_state.hpp      # UNCHANGED: AppState enum (Phase 10)
+-- btop.cpp            # MODIFIED: Replace process_signal_event + if/else if with on_event() + dispatch
+-- btop_input.cpp      # UNCHANGED: Input::poll/get/process remain as-is
+-- btop_input.hpp      # UNCHANGED
```

### Anti-Patterns to Avoid

- **Moving all Reloading/Resizing logic into transition functions:** These states involve multi-step sequences (stop runner, reconfigure, recalculate, trigger redraw). Forcing them into a single `on_event()` return is awkward. Keep them as post-dispatch state processing.
- **Removing the atomic AppState too early:** Phase 12 keeps `atomic<AppState>` for thread safety (the runner thread reads it). Phase 13 replaces it with a variant. Don't remove the atomic in Phase 12.
- **Making KeyInput go through the signal queue:** Key input comes from `pselect()`/`read()` on the main thread, not from signal handlers. It should be dispatched directly to `dispatch_event()`, not pushed through the EventQueue.
- **Breaking the Input::process() / Menu::process() delegation:** Phase 12 should call these existing functions from within `on_event()` handlers, not reimplement their logic. The menu and input systems are Phase v1.2 scope.
- **Attempting to make on_event pure functions:** Some transitions have side effects (calling `clean_quit()`, `Runner::run()`, `_sleep()`). This is expected. Pure functions come in Phase 13 with entry/exit actions.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| State+Event dispatch | Manual nested switch statements | `dispatch_state()` + `std::visit()` two-level dispatch | Compiler-checked exhaustiveness; adding a new event type forces handling |
| Visitor construction | Separate visitor class | `std::overloaded` lambda pattern or nested lambdas | Less boilerplate, same performance |
| Key string in event | `std::string` in event struct | Fixed-size `std::array<char, 32>` buffer | Keeps AppEvent trivially copyable for signal-safe queue |
| Priority enforcement | Complex event sorting/reordering | Early-exit + guard conditions in transition functions | Simpler, matches existing semantics, no allocation |

**Key insight:** The transition functions are thin wrappers around existing code. The `on_event()` overloads mostly call the same functions (`clean_quit()`, `_sleep()`, `_resume()`, `term_resize()`, `Runner::run()`, `Input::process()`) that the current if/else if chain calls. The value is in making the dispatch explicit and typed, not in rewriting the action logic.

## Common Pitfalls

### Pitfall 1: Priority Inversion
**What goes wrong:** A Resize event processed after a Quit event overrides the Quitting state with Resizing, causing the app to not quit.
**Why it happens:** Events are processed FIFO from the queue. If Quit is followed by Resize, the Resize handler might set state back to non-terminal.
**How to avoid:** Early exit from the event drain loop when state becomes Error or Quitting. Guard conditions in transition functions: `on_event(state_tag::Quitting, const event::Resize&)` returns `AppState::Quitting` (no-op via catch-all).
**Warning signs:** App doesn't quit on Ctrl+C when resize events are pending.

### Pitfall 2: Losing the Reloading -> Resizing Chain
**What goes wrong:** Reloading is handled by a transition function that returns a new state, but the code that chains Reloading -> Resizing (after config reinit) is lost.
**Why it happens:** The current code sets `app_state = Resizing` at the end of the reload handler. If this is turned into a simple state return, the resize actions (calcSizes, runner restart) might not execute.
**How to avoid:** Keep Reloading and Resizing as multi-step state processing (not single-event transitions). The `process_accumulated_state()` function handles the chain.
**Warning signs:** Config reload (Ctrl+R or SIGUSR2) doesn't trigger a visual refresh.

### Pitfall 3: KeyInput During Non-Running States
**What goes wrong:** Key input is dispatched to `on_event()` while the app is in Resizing or Reloading state, causing unexpected behavior.
**Why it happens:** The current code only processes input in the inner `for` loop, which is inside the main loop after state processing. If keys are dispatched via `on_event()` during non-Running states, they might interact with partially-updated state.
**How to avoid:** Only generate KeyInput events (call `Input::poll()`) when state is Running. This matches the current behavior where input polling only happens after all state checks pass.
**Warning signs:** Key presses during resize cause crashes or visual corruption.

### Pitfall 4: Clock Update Not Fitting the Event Model
**What goes wrong:** The clock update (`Draw::update_clock()`) currently runs as a periodic check inside the main loop (line 1386), not as an event. Forcing it into the event model creates unnecessary complexity.
**Why it happens:** The clock update is a lightweight operation that checks if the clock display needs refreshing, not a state transition.
**How to avoid:** Keep clock updates as inline checks within the main loop, outside the event dispatch. The clock is a display concern, not a state concern. The `TimerTick` event triggers the full data collection/draw cycle, not the clock widget specifically.
**Warning signs:** Clock stops updating, or updates too frequently.

### Pitfall 5: Breaking the update_ms / future_time Timer Logic
**What goes wrong:** The timer logic (`future_time`, `update_ms`) currently controls both when `Runner::run()` is called and how long `Input::poll()` blocks. Moving `Runner::run()` into a `TimerTick` handler but leaving the poll timeout logic unchanged creates a timing mismatch.
**Why it happens:** The timer and input polling are tightly coupled in the inner loop.
**How to avoid:** Keep the timer/input polling structure intact. Generate a `TimerTick` event when `time_ms() >= future_time` and dispatch it, but don't restructure the inner poll loop. The TimerTick handler calls `Runner::run("all")` and resets `future_time`.
**Warning signs:** Data collection happens at wrong intervals, or input becomes unresponsive.

### Pitfall 6: Forgetting SIGTERM Handling
**What goes wrong:** The current signal handler handles SIGINT but SIGTERM is handled separately (it's not in the Phase 11 signal handler switch). Phase 12 must not break SIGTERM handling.
**Why it happens:** SIGTERM is handled by the default handler or via `atexit()`/`quick_exit()`. It may not flow through the event queue at all.
**How to avoid:** Verify SIGTERM handling path. Currently `clean_quit(-1)` is registered as an exit handler via `std::atexit()` (line 1258 area). This is independent of the event system and should remain unchanged.
**Warning signs:** `kill <pid>` doesn't cleanly shut down btop.

## Code Examples

### Complete Transition Table

All valid (state, event) combinations and their transitions:

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| Running | Quit | Runner::active | Runner::stopping = true | Quitting |
| Running | Quit | !Runner::active | clean_quit(exit_code) | (exits) |
| Running | Sleep | Runner::active | Runner::stopping = true | Sleeping |
| Running | Sleep | !Runner::active | _sleep() | Running |
| Running | Resume | - | _resume() | Running |
| Running | Resize | - | term_resize() | (check Resizing) |
| Running | Reload | - | (none - deferred to accumulated) | Reloading |
| Running | TimerTick | !Resizing | Runner::run("all") | Running |
| Running | KeyInput | Menu::active | Menu::process(key) | Running |
| Running | KeyInput | !Menu::active | Input::process(key) | Running |
| Running | ThreadError | - | (none) | Error |
| Resizing | any signal event | - | (queue for later) | Resizing |
| Reloading | any signal event | - | (queue for later) | Reloading |
| Sleeping | Resume | - | (handled in accumulated) | Running |
| Quitting | ThreadError | - | (escalate) | Error |
| Quitting | (any other) | - | (no-op) | Quitting |
| Error | (any) | - | (no-op) | Error |

### State Tag Types and Dispatch Helper (btop_events.hpp addition)

```cpp
// State tag types for compile-time dispatch
// These mirror the AppState enum but as types for overload resolution
namespace state_tag {
    struct Running {};
    struct Resizing {};
    struct Reloading {};
    struct Sleeping {};
    struct Quitting {};
    struct Error {};
}

// Convert runtime AppState enum to compile-time state tag dispatch
template<typename Visitor>
decltype(auto) dispatch_state(Global::AppState s, Visitor&& vis) {
    using Global::AppState;
    switch (s) {
        case AppState::Running:   return std::forward<Visitor>(vis)(state_tag::Running{});
        case AppState::Resizing:  return std::forward<Visitor>(vis)(state_tag::Resizing{});
        case AppState::Reloading: return std::forward<Visitor>(vis)(state_tag::Reloading{});
        case AppState::Sleeping:  return std::forward<Visitor>(vis)(state_tag::Sleeping{});
        case AppState::Quitting:  return std::forward<Visitor>(vis)(state_tag::Quitting{});
        case AppState::Error:     return std::forward<Visitor>(vis)(state_tag::Error{});
    }
    __builtin_unreachable();
}
```

### KeyInput Event Struct (btop_events.hpp addition)

```cpp
namespace event {
    struct KeyInput {
        std::array<char, 32> key_data{};
        std::uint8_t key_len{0};

        KeyInput() noexcept = default;

        explicit KeyInput(std::string_view k) noexcept
            : key_len(static_cast<std::uint8_t>(std::min(k.size(), std::size_t{31}))) {
            std::copy_n(k.data(), key_len, key_data.data());
        }

        std::string_view key() const noexcept {
            return {key_data.data(), key_len};
        }
    };
}

// Updated variant:
using AppEvent = std::variant<
    event::TimerTick,
    event::Resize,
    event::Quit,
    event::Sleep,
    event::Resume,
    event::Reload,
    event::ThreadError,
    event::KeyInput
>;
```

### Main Loop Dispatch (btop.cpp)

```cpp
// Dispatch a single event against current state
static AppState dispatch_event(AppState current, const AppEvent& ev) {
    return dispatch_state(current, [&](auto tag) {
        return std::visit([&](const auto& event) {
            return on_event(tag, event);
        }, ev);
    });
}

// Main loop (simplified):
while (true) {
    auto state = Global::app_state.load(std::memory_order_acquire);

    // 1. Drain signal events
    while (auto ev = Global::event_queue.try_pop()) {
        state = dispatch_event(state, *ev);
        Global::app_state.store(state);
        if (state == AppState::Quitting or state == AppState::Error) break;
    }

    // 2. Process accumulated state (multi-step actions for Reloading/Resizing/etc.)
    state = process_accumulated_state(state);
    Global::app_state.store(state);

    // 3. Timer tick
    if (time_ms() >= future_time and state == AppState::Running) {
        state = dispatch_event(state, event::TimerTick{});
        Global::app_state.store(state);
        update_ms = Config::getI(IntKey::update_ms);
        future_time = time_ms() + update_ms;
    }

    // 4. Clock update (not an event -- display concern)
    if (state == AppState::Running and Draw::update_clock() and not Menu::active) {
        Runner::run("clock");
    }

    // 5. Input polling
    for (auto current_time = time_ms();
         current_time < future_time and state == AppState::Running;
         current_time = time_ms()) {
        if (std::cmp_not_equal(update_ms, Config::getI(IntKey::update_ms))) {
            update_ms = Config::getI(IntKey::update_ms);
            future_time = time_ms() + update_ms;
        }
        else if (future_time - current_time > update_ms) {
            future_time = current_time;
        }
        else if (Input::poll(min(1000ULL, future_time - current_time))) {
            if (not Runner::active) Config::unlock();
            auto key = Input::get();
            if (not key.empty()) {
                state = dispatch_event(state, event::KeyInput{key});
                Global::app_state.store(state);
            }
        }
        else break;
    }
}
```

### Transition Function Examples (btop.cpp)

```cpp
// --- Running state handlers ---
static AppState on_event(state_tag::Running, const event::Quit& e) {
    if (Runner::active) {
        Runner::stopping = true;
        return AppState::Quitting;
    }
    clean_quit(e.exit_code);
    return AppState::Quitting;
}

static AppState on_event(state_tag::Running, const event::Sleep&) {
    if (Runner::active) {
        Runner::stopping = true;
        return AppState::Sleeping;
    }
    _sleep();
    return AppState::Running;
}

static AppState on_event(state_tag::Running, const event::Resume&) {
    _resume();
    return AppState::Running;
}

static AppState on_event(state_tag::Running, const event::Resize&) {
    term_resize();
    return AppState::Resizing;
}

static AppState on_event(state_tag::Running, const event::Reload&) {
    return AppState::Reloading;
}

static AppState on_event(state_tag::Running, const event::TimerTick&) {
    Runner::run("all");
    return AppState::Running;
}

static AppState on_event(state_tag::Running, const event::KeyInput& e) {
    if (Menu::active) {
        Menu::process(e.key());
    } else {
        Input::process(e.key());
    }
    return AppState::Running;
}

static AppState on_event(state_tag::Running, const event::ThreadError&) {
    return AppState::Error;
}

// --- Catch-all: unhandled combinations are no-ops ---
static AppState on_event(const auto&, const auto&) {
    // This requires passing current state -- see note below
}
```

**Note on catch-all:** The catch-all needs the current state to return it. Two approaches:
1. Pass `AppState current` as a third parameter to all `on_event()` overloads
2. Have `dispatch_event()` compare before/after and use current if no specific overload matched

Recommended: approach 1 -- add `AppState current` parameter:

```cpp
// All on_event overloads take current state as final parameter
static AppState on_event(state_tag::Running, const event::Quit& e, AppState) { ... }

// Catch-all returns current state unchanged
static AppState on_event(const auto&, const auto&, AppState current) {
    return current;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Flag polling (`if (flag) { ... }`) | Event-driven dispatch (`std::visit`) | C++17 variant/visit | Compile-time exhaustiveness, explicit transitions |
| Inline if/else if chains | Named transition functions | General software engineering | Testable, documented, discoverable |
| Two-step event processing (drain -> check state) | Single-step dispatch (event -> transition -> new state) | Phase 12 | Eliminates intermediate atomic writes, clearer data flow |
| Manual priority ordering via code position | Priority via early-exit + guard conditions | Phase 12 | Same semantics, more explicit about why |

## Open Questions

1. **Should `event::KeyInput` use `std::string_view` into `Input::input` instead of copying?**
   - What we know: The `Input::input` buffer is stable during the main loop iteration (it's only overwritten by the next `Input::poll()` call).
   - What's unclear: Whether holding a `string_view` into `Input::input` is safe if `on_event()` triggers a nested `Input::poll()` call (it doesn't currently, but future changes might).
   - Recommendation: Use the fixed-size `std::array<char, 32>` copy. It's safe, simple, and the 32-byte copy is negligible compared to the I/O operations in the transition functions. This also keeps the variant trivially copyable.

2. **Should `process_accumulated_state()` be transitioned to event-driven dispatch too?**
   - What we know: Reloading and Resizing involve multi-step sequences that are more like "state entry actions" than event responses.
   - What's unclear: Whether forcing these into the event model adds clarity or complexity.
   - Recommendation: Keep them as procedural functions in Phase 12. Phase 13 introduces formal entry/exit actions on state transitions, which is the natural place to refactor these. For Phase 12, the goal is extracting the event dispatch, not the state processing.

3. **Should the clock update be an event?**
   - What we know: `Draw::update_clock()` is a lightweight check-and-draw. It runs every loop iteration.
   - What's unclear: Whether wrapping it as an event adds value or just noise.
   - Recommendation: No. Keep it as an inline check. It's not a state transition stimulus -- it's a display concern. Phase 12 should not over-engineer the event model.

4. **How to handle `Input::process()` calling `clean_quit()` directly (e.g., when user presses 'q')?**
   - What we know: `Input::process()` calls `clean_quit(0)` when the 'q' key is pressed (btop_input.cpp line 228). This bypasses the FSM entirely.
   - What's unclear: Whether to change this to return an intent (like `AppState::Quitting`) or keep the direct call.
   - Recommendation: Keep the direct `clean_quit()` call for Phase 12. Changing `Input::process()` to return state-change intents is a larger refactor that belongs in Phase v1.2 (Input FSM). For Phase 12, `on_event(Running, KeyInput)` delegates to `Input::process()` which may call `clean_quit()` directly -- this preserves existing behavior.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 (via FetchContent) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R Transition --output-on-failure` |
| Full suite command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TRANS-01 | on_event() overloads exist for each state+event pair | unit | `ctest --test-dir build/tests -R Transition --output-on-failure` | Wave 0 |
| TRANS-01 | Running+Quit returns Quitting | unit | `ctest --test-dir build/tests -R Transition --output-on-failure` | Wave 0 |
| TRANS-01 | Running+Resize returns Resizing | unit | `ctest --test-dir build/tests -R Transition --output-on-failure` | Wave 0 |
| TRANS-01 | Running+Reload returns Reloading | unit | `ctest --test-dir build/tests -R Transition --output-on-failure` | Wave 0 |
| TRANS-01 | Running+ThreadError returns Error | unit | `ctest --test-dir build/tests -R Transition --output-on-failure` | Wave 0 |
| TRANS-02 | dispatch_event() routes to correct on_event() overload | unit | `ctest --test-dir build/tests -R Dispatch --output-on-failure` | Wave 0 |
| TRANS-02 | dispatch_state() converts all enum values to tags | unit | `ctest --test-dir build/tests -R Dispatch --output-on-failure` | Wave 0 |
| TRANS-04 | Quitting state ignores Resize events (priority) | unit | `ctest --test-dir build/tests -R Priority --output-on-failure` | Wave 0 |
| TRANS-04 | Error state ignores all events (terminal) | unit | `ctest --test-dir build/tests -R Priority --output-on-failure` | Wave 0 |
| TRANS-04 | Early exit stops processing after Quitting | unit | `ctest --test-dir build/tests -R Priority --output-on-failure` | Wave 0 |
| EVENT-04 | Main loop dispatches signal events to transition functions | integration (manual) | Build + btop runs identically to before | manual |
| EVENT-04 | Main loop dispatches timer events to transition functions | integration (manual) | Build + btop timer behavior unchanged | manual |
| EVENT-04 | Main loop dispatches key input to transition functions | integration (manual) | Build + key input works identically | manual |
| EVENT-04 | KeyInput event is trivially copyable and fits in variant | unit | `ctest --test-dir build/tests -R Event --output-on-failure` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure`
- **Per wave merge:** Full suite + manual build and test on macOS
- **Phase gate:** Full suite green + manual verification: resize, Ctrl+C, Ctrl+Z/fg, kill -USR2, key input, menu navigation

### Wave 0 Gaps
- [ ] `tests/test_transitions.cpp` -- covers TRANS-01, TRANS-02, TRANS-04 (transition function unit tests using local state, no dependency on btop.cpp globals)
- [ ] Update `tests/CMakeLists.txt` -- add `test_transitions.cpp` to `btop_test` sources
- [ ] `tests/test_events.cpp` -- update existing tests for new KeyInput event type in variant

**Important test design constraint:** Transition function tests must NOT depend on btop.cpp globals (`Runner::active`, `Menu::active`, etc.). Tests should verify the dispatch mechanism (state_tag + event -> AppState return value) and the guard-free transitions. Tests for transitions with guards (Runner::active) require either:
1. Testing only the dispatch path (tag -> correct overload), not the guard logic
2. Abstracting the guards behind an injectable interface (overkill for Phase 12)

Recommended: test the pure dispatch (correct overload resolution) and the non-guarded transitions. Guard-dependent transitions are verified manually and in Phase 15's integration tests.

## Sources

### Primary (HIGH confidence)
- Source code analysis of `src/btop.cpp` lines 295-357 (current process_signal_event overloads + signal handler)
- Source code analysis of `src/btop.cpp` lines 1327-1428 (current main loop with event drain + if/else if chain)
- Source code analysis of `src/btop.cpp` lines 134-280 (term_resize, clean_quit, _sleep, _resume functions)
- Source code analysis of `src/btop.cpp` lines 466-895 (Runner namespace: _runner thread, run(), stop())
- Source code analysis of `src/btop_events.hpp` (AppEvent variant, EventQueue class from Phase 11)
- Source code analysis of `src/btop_state.hpp` (AppState enum from Phase 10)
- Source code analysis of `src/btop_input.cpp` lines 99-217 (poll, get, interrupt functions)
- Source code analysis of `src/btop_input.cpp` lines 218-370 (Input::process key handling)
- `.planning/research/AUTOMATA-ARCHITECTURE.md` sections 3.1 and 6.3 (variant+visit pattern, phase 3 plan)
- `.planning/phases/11-event-queue/11-RESEARCH.md` (event types, queue design, forward compatibility notes)
- `.planning/REQUIREMENTS.md` (TRANS-01, TRANS-02, TRANS-04, EVENT-04 definitions)
- `.planning/STATE.md` (Phase 11 decisions on event queue design)

### Secondary (MEDIUM confidence)
- [cppreference: std::visit](https://en.cppreference.com/w/cpp/utility/variant/visit) - Multi-variant visit semantics, return type requirements
- [cppreference: std::variant](https://en.cppreference.com/w/cpp/utility/variant) - Variant size limits, trivially copyable conditions
- [Ben Deane - CppCon 2017: Using Types Effectively](https://www.youtube.com/watch?v=ojZbFIQSdl8) - State machine patterns with std::variant
- [Mateusz Pusz - CppCon 2018: Effective replacement of dynamic polymorphism with std::variant](https://www.youtube.com/watch?v=gKbORJtnVu8) - Overload resolution patterns with visit

### Tertiary (LOW confidence)
- None. All findings are from direct source code analysis and C++ standard documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Using only std::visit and std::variant already present in the codebase; no new libraries
- Architecture: HIGH - The dispatch_state + std::visit pattern is a well-understood C++ idiom; the codebase already uses std::visit in the event drain loop (Phase 11); the transition table is fully enumerable from source code analysis
- Pitfalls: HIGH - All pitfalls identified from actual main loop analysis (priority preservation, Reloading->Resizing chain, KeyInput timing, clock update coupling)

**Research date:** 2026-02-28
**Valid until:** 2026-03-28 (stable -- no external dependencies, pure internal refactoring using C++ standard library)
