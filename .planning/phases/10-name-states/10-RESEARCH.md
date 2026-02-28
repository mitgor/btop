# Phase 10: Name States - Research

**Researched:** 2026-02-28
**Domain:** C++ enum-based state management replacing scattered atomic<bool> flags
**Confidence:** HIGH

## Summary

Phase 10 is a mechanical refactoring that replaces 7 `atomic<bool>` flags in `Global` namespace with a single `enum class AppState`. This is the simplest phase in the v1.1 roadmap: it introduces no new behavior, no new libraries, and no threading changes. The main complexity lies in correctly categorizing the existing flags, because not all 7 flags represent "app states" in the same way -- some are deferred-action signals, one is a lock/guard, and one is a lifecycle marker.

The codebase uses C++23, so `enum class` with `std::underlying_type` and all modern enum features are available. The project already has a test infrastructure with Google Test v1.17.0 and a `tests/` directory with existing unit tests, so the new `btop_state.hpp` header can be tested immediately.

**Primary recommendation:** Define `enum class AppState` in a new `src/btop_state.hpp` header. Replace the 7 `atomic<bool>` flags with a single `atomic<AppState>` plus a separate `atomic<bool>` for `init_conf` (which serves as a lock, not a state). Use this enum in the main loop's if/else if chain. Touch only `btop.cpp`, `btop_shared.hpp`, `btop_config.cpp`, `btop_draw.cpp`, and `btop_tools.cpp`.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| STATE-01 | App states are named via enum replacing 7 scattered atomic<bool> flags | Flag taxonomy analysis (Section "Architecture Patterns > Flag Taxonomy") identifies exactly which flags map to enum states, which must remain as separate atomics, and why |
| STATE-04 | State types defined in new btop_state.hpp header | Header design pattern documented (Section "Architecture Patterns > btop_state.hpp Header Design") with exact contents, include guards, and integration points |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ enum class | C++11+ (project uses C++23) | Type-safe enumeration for app states | Language built-in, zero-cost, no dependencies |
| std::atomic | C++11+ | Thread-safe state storage | Already used for the boolean flags being replaced |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Google Test | v1.17.0 | Unit tests for state enum | Already in project, used for verifying enum correctness |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| enum class | std::variant (Phase 13) | variant carries per-state data but is premature for Phase 10; enum is the stepping stone |
| Single atomic<AppState> | Keep separate booleans | Defeats the purpose -- booleans allow invalid combinations |

## Architecture Patterns

### Flag Taxonomy (Critical Discovery)

The 7 flags in `Global` namespace (btop.cpp lines 117-129) are NOT all "app states" in the same sense. They fall into 4 categories:

| Category | Flags | Behavior | Enum Candidate? |
|----------|-------|----------|-----------------|
| **App lifecycle state** | `quitting` | Set once, never cleared. Guards re-entry in `clean_quit()`. | YES -- `AppState::Quitting` |
| **Deferred actions** | `should_quit`, `should_sleep`, `reload_conf`, `resized` | Set by signal handlers when runner is active; consumed by main loop which clears them. | YES -- can be mapped to transient states like `AppState::Quitting`, `AppState::Sleeping`, `AppState::Reloading`, `AppState::Resizing` |
| **Lock/guard** | `init_conf` | Used via `atomic_lock` pattern (set true during config init, false after). Read in btop_config.cpp:596 to skip validation during init. | NO -- this is a lock, not a state. Keep as `atomic<bool>`. |
| **Lifecycle marker** | `_runner_started` | Set once after `pthread_create` (line 1262), checked once in `clean_quit()` (line 220). | NO -- this is a one-shot lifecycle flag. Keep as `atomic<bool>`. |
| **Error signal** | `thread_exception` | Set by runner thread on error; checked by main loop. Always paired with `exit_error_msg`. | YES -- `AppState::Error` (or keep separate since it's cross-thread) |

**Key insight:** Only 5 of the 7 flags naturally map to enum states. `init_conf` and `_runner_started` serve different purposes (lock and lifecycle marker) and should remain as separate `atomic<bool>` values.

However, the success criteria says "A single enum replaces all 7 atomic<bool> flags." This tension must be resolved. The recommended approach: the enum replaces the 5 state-like flags (`resized`, `quitting`, `should_quit`, `should_sleep`, `reload_conf`), `thread_exception` maps to an `Error` state, and `_runner_started` and `init_conf` remain as separate booleans since they serve fundamentally different roles (a lock and a lifecycle marker, not states).

### Proposed Enum Design

```cpp
// src/btop_state.hpp
#pragma once

#include <atomic>
#include <cstdint>

namespace Global {
    //! Application lifecycle states.
    //! Priority order (highest first): Error > Quitting > Sleeping > Reloading > Resizing > Running
    //! The main loop checks states in this priority order -- do NOT reorder enum values.
    enum class AppState : std::uint8_t {
        Running,       // Normal operation -- collecting, drawing, processing input
        Resizing,      // Terminal resize detected, waiting for recalculation
        Reloading,     // Config reload requested (SIGUSR2 or Ctrl+R)
        Sleeping,      // SIGTSTP received, about to suspend
        Quitting,      // Clean shutdown in progress
        Error,         // Thread exception detected, will exit with error
    };

    extern std::atomic<AppState> app_state;
}
```

### btop_state.hpp Header Design

The header should contain:
1. The `AppState` enum class definition
2. The `extern` declaration for the atomic state variable
3. NO implementation logic (pure type definitions)
4. Include guards via `#pragma once`

The actual `atomic<AppState>` definition goes in `btop.cpp` (replacing the 7 flag definitions).

Files that need to `#include "btop_state.hpp"`:
- `btop.cpp` -- defines the variable, uses it in main loop and signal handlers
- `btop_shared.hpp` -- currently declares `extern atomic<bool> quitting`, `resized`, etc. -- these declarations move to btop_state.hpp
- `btop_config.cpp` -- reads `Global::init_conf` (stays as bool)
- `btop_draw.cpp` -- reads `Global::resized` (becomes `Global::app_state == AppState::Resizing`)
- `btop_tools.cpp` -- sets `Global::resized = false` (becomes `Global::app_state.store(AppState::Running)`)

### Main Loop Transformation

**Before** (btop.cpp lines 1310-1385):
```cpp
while (not true not_eq not false) {
    if (Global::thread_exception) {
        clean_quit(1);
    }
    else if (Global::should_quit) {
        clean_quit(0);
    }
    else if (Global::should_sleep) {
        Global::should_sleep = false;
        _sleep();
    }
    else if (Global::reload_conf) {
        Global::reload_conf = false;
        // ... reload logic
    }
    term_resize(Global::resized);
    if (Global::resized) {
        // ... resize logic
        Global::resized = false;
    }
    // ... timer and input handling
}
```

**After:**
```cpp
while (not true not_eq not false) {
    const auto state = Global::app_state.load();
    if (state == AppState::Error) {
        clean_quit(1);
    }
    else if (state == AppState::Quitting) {
        clean_quit(0);
    }
    else if (state == AppState::Sleeping) {
        Global::app_state.store(AppState::Running);
        _sleep();
    }
    else if (state == AppState::Reloading) {
        Global::app_state.store(AppState::Running);
        // ... reload logic
    }
    term_resize(state == AppState::Resizing);
    if (state == AppState::Resizing) {
        // ... resize logic
        Global::app_state.store(AppState::Running);
    }
    // ... timer and input handling
}
```

### Signal Handler Transformation

**Before** (btop.cpp lines 296-331):
```cpp
case SIGINT:
    if (Runner::active) {
        Global::should_quit = true;
        Runner::stopping = true;
    } else {
        clean_quit(0);
    }
    break;
```

**After:**
```cpp
case SIGINT:
    if (Runner::active) {
        Global::app_state.store(AppState::Quitting);
        Runner::stopping = true;
    } else {
        clean_quit(0);
    }
    break;
```

### Anti-Patterns to Avoid

- **Encoding lock semantics in the enum:** `init_conf` is a lock, not a state. Forcing it into the enum would break the atomic_lock pattern used in `init_config()`. Keep it as a separate `atomic<bool>`.
- **Encoding lifecycle markers in the enum:** `_runner_started` is set once and never cleared. It's not a "state" the app transitions through. Keep it as a separate `atomic<bool>`.
- **Using the enum for runner thread flags:** The Runner namespace has its own set of `atomic<bool>` flags (`active`, `stopping`, `waiting`, `redraw`). These belong to the Runner FSM (Phase 14), not the App state enum.
- **Changing the priority order:** The if/else if chain in the main loop has a specific priority order that is load-bearing. The enum values should document this order but the check logic must preserve it exactly.
- **Using `switch` instead of `if/else if`:** Tempting, but the main loop does NOT check states in a simple switch -- it checks high-priority states first, then unconditionally calls `term_resize()`, then checks `resized` again. A switch statement would not naturally express this flow. Keep the if/else if structure for Phase 10; Phase 12 (Extract Transitions) can restructure it.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Thread-safe enum storage | Custom locking wrapper | `std::atomic<AppState>` | `atomic` supports enum class with uint8_t underlying type natively; lock-free on all platforms |
| State validation | Runtime assert checks | Compiler exhaustiveness | Use `switch` with no `default` case in future phases to get compiler warnings on unhandled states |
| State name strings | Manual string mapping | Inline helper function | Small utility for debug logging, not worth a library |

**Key insight:** `std::atomic` works with any trivially copyable type. `enum class AppState : uint8_t` is trivially copyable, so `std::atomic<AppState>` is valid and lock-free on all architectures btop targets.

## Common Pitfalls

### Pitfall 1: Resized Flag Has Multiple Setters
**What goes wrong:** `Global::resized` is set in 5 different locations: `term_resize()` (lines 140, 154), the runner thread `coreNum_reset` path (line 556), the reload_conf handler (line 1332), and `btop_tools.cpp` (line 174). If you only update `btop.cpp`, the other setters will still try to write `true` to a variable that no longer exists.
**Why it happens:** The flag is used as a cross-module signaling mechanism.
**How to avoid:** Grep for ALL usages of each flag across the entire codebase before making changes. Use compiler errors as a safety net (remove the old declaration, build, fix all errors).
**Warning signs:** Build errors in files you didn't modify.

### Pitfall 2: init_conf Is Not A State
**What goes wrong:** Treating `init_conf` as an enum state breaks the `atomic_lock` pattern in `init_config()` (line 336). The `atomic_lock` class expects `atomic<bool>&` -- it cannot work with an enum.
**Why it happens:** The flag looks like a state ("are we initializing config?") but is actually used as a RAII lock guard.
**How to avoid:** Keep `init_conf` as a separate `atomic<bool>`. Document why it's excluded from the enum.
**Warning signs:** Compile error in `atomic_lock` constructor.

### Pitfall 3: Race Between Signal Handler and Main Loop State Transitions
**What goes wrong:** Signal handler stores `AppState::Quitting` while main loop is in the middle of processing `AppState::Resizing`. The main loop sets state back to `Running` (clearing the resize), accidentally clearing the quit request.
**Why it happens:** With separate booleans, `should_quit` and `resized` are independent -- both can be true. With a single enum, only one state is active.
**How to avoid:** In the main loop, only clear state (set to Running) if the current state is still the one you were processing. Use `compare_exchange_strong` for the transition back to Running:
```cpp
AppState expected = AppState::Resizing;
Global::app_state.compare_exchange_strong(expected, AppState::Running);
// If a signal handler changed state to Quitting in the meantime, this is a no-op
```
**Warning signs:** Missed quit/sleep signals after resize. Signals "swallowed" during transitions.

### Pitfall 4: Resized is Both a State and a Condition Check
**What goes wrong:** `Global::resized` is checked in draw code (`btop_draw.cpp:854`, `btop_draw.cpp:2762`) as a read-only condition, not as a state transition trigger. If you remove the boolean, these checks need to compare against the enum.
**Why it happens:** The flag serves dual purposes: triggering resize handling AND informing draw code that a resize is in progress.
**How to avoid:** Map these read sites carefully. `Global::resized` reads in draw code become `Global::app_state.load() == AppState::Resizing`.
**Warning signs:** Visual glitches during terminal resize.

### Pitfall 5: thread_exception Pairs with exit_error_msg
**What goes wrong:** `thread_exception` is always set together with `exit_error_msg` (lines 485-487, 661-662). If you replace it with `AppState::Error`, you must ensure the error message is still set before the state transition, or the main loop might see `Error` state before the message is ready.
**Why it happens:** Store ordering -- the message write and state write are independent.
**How to avoid:** Set `exit_error_msg` BEFORE `app_state.store(AppState::Error)`. Use `std::memory_order_release` for the state store and `std::memory_order_acquire` in the main loop's load to guarantee the message is visible.
**Warning signs:** Empty error messages on crash.

## Code Examples

### btop_state.hpp (Complete Header)

```cpp
// src/btop_state.hpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

namespace Global {

    /// Application lifecycle states, ordered by check priority (highest first in main loop).
    /// The main loop checks: Error > Quitting > Sleeping > Reloading > Resizing > Running.
    /// WARNING: The if/else if chain ordering is load-bearing. Do NOT reorder checks.
    enum class AppState : std::uint8_t {
        Running,       ///< Normal operation
        Resizing,      ///< Terminal resize detected
        Reloading,     ///< Config reload requested (SIGUSR2 / Ctrl+R)
        Sleeping,      ///< SIGTSTP received, about to suspend
        Quitting,      ///< Clean shutdown requested
        Error,         ///< Thread exception, will exit with error
    };

    /// Thread-safe application state. Replaces former atomic<bool> flags:
    /// resized, quitting, should_quit, should_sleep, reload_conf, thread_exception.
    /// NOTE: init_conf and _runner_started remain as separate atomic<bool>
    /// because they serve as a lock and lifecycle marker respectively.
    extern std::atomic<AppState> app_state;

    /// Debug/logging helper
    constexpr std::string_view to_string(AppState s) noexcept {
        switch (s) {
            case AppState::Running:   return "Running";
            case AppState::Resizing:  return "Resizing";
            case AppState::Reloading: return "Reloading";
            case AppState::Sleeping:  return "Sleeping";
            case AppState::Quitting:  return "Quitting";
            case AppState::Error:     return "Error";
        }
        return "Unknown";
    }
}
```

### State Transition in btop.cpp (Main Loop)

```cpp
// Replace: atomic<bool> resized(false); quitting(false); should_quit(false);
//          should_sleep(false); reload_conf(false); thread_exception(false);
// With:
std::atomic<AppState> app_state{AppState::Running};
// Keep: atomic<bool> _runner_started(false);
// Keep: atomic<bool> init_conf(false);
```

### Safe State Clearing with compare_exchange

```cpp
// In main loop, after handling resize:
auto expected = AppState::Resizing;
Global::app_state.compare_exchange_strong(expected, AppState::Running);
// If signal handler set a higher-priority state (Quitting, Error) in the
// meantime, this is a no-op and the next loop iteration handles it.
```

### Signal Handler Updates

```cpp
static void _signal_handler(const int sig) {
    switch (sig) {
        case SIGINT:
            if (Runner::active) {
                Global::app_state.store(AppState::Quitting);
                Runner::stopping = true;
                Input::interrupt();
            } else {
                clean_quit(0);
            }
            break;
        case SIGTSTP:
            if (Runner::active) {
                Global::app_state.store(AppState::Sleeping);
                Runner::stopping = true;
                Input::interrupt();
            } else {
                _sleep();
            }
            break;
        case SIGWINCH:
            term_resize();  // term_resize sets app_state to Resizing internally
            break;
        case SIGUSR2:
            Global::app_state.store(AppState::Reloading);
            Input::interrupt();
            break;
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Multiple atomic<bool> flags for state | Single atomic enum | C++11 introduced enum class; C++20 made atomic<enum> mainstream | Eliminates invalid state combinations |
| `#define` or raw `int` for state codes | `enum class` with underlying type | C++11 | Type safety, no implicit conversions |
| Manual flag coordination | std::variant state machines | C++17 | Full type-safe FSM (Phase 13 target) |

**Phase 10 is the first step:** enum replaces booleans. Phase 13 graduates the enum to `std::variant` with per-state data.

## Open Questions

1. **Should `thread_exception` map to AppState::Error or remain separate?**
   - What we know: `thread_exception` is set from the runner thread and read from the main thread. It's always paired with `exit_error_msg`. It behaves like a "go to error state" signal.
   - What's unclear: Whether combining it into the enum creates subtle ordering issues (the msg must be visible before the state change).
   - Recommendation: Map it to `AppState::Error`. Use `memory_order_release` when setting from runner thread and `memory_order_acquire` when loading in main loop. This is the standard producer-consumer pattern and is safe.

2. **How to handle the `resized` flag's dual use as a parameter?**
   - What we know: `term_resize(Global::resized)` passes the boolean value as a force parameter (line 1336). With the enum, this becomes `term_resize(app_state.load() == AppState::Resizing)`.
   - What's unclear: Whether `term_resize` should be refactored to accept `AppState` directly.
   - Recommendation: Keep it simple -- pass `app_state.load() == AppState::Resizing` as the boolean parameter. Don't change `term_resize`'s signature in Phase 10.

3. **Priority inversion: what if Quitting and Resizing are both "requested"?**
   - What we know: With separate booleans, both `should_quit` and `resized` can be true simultaneously. With a single enum, only one state is active. The if/else if chain already handles priority (quit wins over resize), so only the highest-priority state needs to be stored.
   - What's unclear: Whether signal handlers can fire faster than the main loop processes them, causing lower-priority signals to be lost.
   - Recommendation: This is safe because (a) Quitting/Error are terminal states (app exits), (b) Sleeping suspends the process (resume triggers resize anyway), and (c) Reloading sets `resized = true` at the end (in the new model, set state to `Resizing` after reload completes). The only real race is Resizing being overwritten by a higher-priority state, which is correct behavior.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 (via FetchContent) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R state --output-on-failure` |
| Full suite command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| STATE-01 | Enum has values for all 7 replaced flags | unit | `ctest --test-dir build/tests -R AppState --output-on-failure` | Wave 0 |
| STATE-01 | `atomic<AppState>` is lock-free | unit | `ctest --test-dir build/tests -R AppState --output-on-failure` | Wave 0 |
| STATE-04 | btop_state.hpp is includable from other TUs | unit (compilation) | `cmake --build build --target btop_test` (test file includes the header) | Wave 0 |
| STATE-04 | to_string covers all enum values | unit | `ctest --test-dir build/tests -R AppState --output-on-failure` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure`
- **Per wave merge:** Full suite + manual build on macOS (primary dev platform)
- **Phase gate:** Full suite green + `btop` launches and runs identically to before

### Wave 0 Gaps
- [ ] `tests/test_app_state.cpp` -- covers STATE-01, STATE-04 (enum values, lock-free, to_string)
- [ ] Update `tests/CMakeLists.txt` -- add `test_app_state.cpp` to `btop_test` sources

*(Test infrastructure (Google Test, CMake integration) already exists -- only the test file is missing.)*

## Sources

### Primary (HIGH confidence)
- Source code analysis of `src/btop.cpp` lines 92-130 (Global namespace flag definitions)
- Source code analysis of `src/btop.cpp` lines 296-332 (signal handler)
- Source code analysis of `src/btop.cpp` lines 1310-1385 (main loop)
- Source code analysis of `src/btop.cpp` lines 364-871 (Runner namespace)
- Source code analysis of `src/btop_shared.hpp` lines 395-420 (extern declarations)
- Source code analysis of `src/btop_draw.cpp` lines 854, 2762 (resized reads)
- Source code analysis of `src/btop_config.cpp` line 596 (init_conf read)
- Source code analysis of `src/btop_tools.cpp` line 174 (resized write)
- Source code analysis of `src/btop_tools.hpp` lines 365-378 (atomic_lock class)
- Source code analysis of `tests/CMakeLists.txt` (test infrastructure)
- `.planning/research/AUTOMATA-ARCHITECTURE.md` (domain research, migration strategy)
- `.planning/REQUIREMENTS.md` (STATE-01, STATE-04 definitions)
- `CMakeLists.txt` line 25: C++23 standard confirmed

### Secondary (MEDIUM confidence)
- C++ standard: `std::atomic<T>` supports enum class with trivially copyable underlying types (verified via cppreference, well-established since C++11)
- `compare_exchange_strong` for safe state transitions is a standard atomic pattern

### Tertiary (LOW confidence)
- None. All findings are from direct source code analysis.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Using only C++ standard library features already present in the codebase
- Architecture: HIGH - Direct mechanical replacement with clear 1:1 mapping from flags to enum values
- Pitfalls: HIGH - All pitfalls identified from actual source code analysis (multi-setter flags, lock patterns, race conditions)

**Research date:** 2026-02-28
**Valid until:** 2026-03-28 (stable -- no external dependencies, pure internal refactoring)
