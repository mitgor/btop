# Phase 11: Event Queue - Research

**Researched:** 2026-02-28
**Domain:** Lock-free event queue for signal-to-main-loop communication in C++ POSIX programs
**Confidence:** HIGH

## Summary

Phase 11 introduces a typed event system and a signal-safe event queue that decouples signal handlers from the main loop. Currently, signal handlers directly mutate `Global::app_state` (an `atomic<AppState>`) and call functions like `term_resize()`, `_sleep()`, `Input::interrupt()`, and `clean_quit()`. Some of these calls are not async-signal-safe (e.g., `term_resize` calls `atomic_lock`, `Term::refresh`, and complex UI logic when `Input::polling` is false). Phase 11 makes signal handlers trivially safe: each handler pushes a small, trivially-copyable event struct into a fixed-size lock-free ring buffer, and the main loop pops and processes events on each iteration.

The key technical challenge is designing the event queue to be **async-signal-safe on the push side** (signal handlers are the producers) while remaining efficient on the pop side (main loop is the sole consumer). This is a classic SPSC (single-producer, single-consumer) pattern where the "producer" is a signal handler context. The C++ standard (since C++17) explicitly allows lock-free atomic operations inside signal handlers, so an SPSC ring buffer using `std::atomic<uint32_t>` head/tail indices with a fixed-size array of trivially-copyable event structs is both safe and efficient.

Events are defined as `std::variant` of small POD structs -- but crucially, the variant used inside the queue must be **trivially copyable** (no `std::string` members). Event types that need string data (like `ThreadError`) store a flag in the event and leave the actual string in `Global::exit_error_msg`, preserving the existing pattern with proper memory ordering.

**Primary recommendation:** Implement a fixed-capacity (32 slots) SPSC ring buffer in a new `src/btop_events.hpp` header. Define event types as a `std::variant` of trivially-copyable structs. Signal handlers push events with a single atomic store. The main loop drains the queue at the top of each iteration, processing events in FIFO order but respecting priority by sorting/scanning. Preserve `Input::interrupt()` (SIGUSR1 -> pselect wakeup) as the notification mechanism to break the main loop out of `pselect()` when events arrive.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| EVENT-01 | Events defined as std::variant of typed event structs (TimerTick, KeyInput, Resize, Signal, ConfigReload, ThreadError) | Event type taxonomy (Section "Architecture Patterns > Event Type Design") defines all 6 event structs as trivially-copyable POD types fitting in a variant; Section "Code Examples" provides complete definitions |
| EVENT-02 | Lock-free event queue for signal handler -> main loop communication | SPSC ring buffer design (Section "Architecture Patterns > Lock-Free SPSC Ring Buffer") uses atomic head/tail with fixed array; async-signal-safety verified per C++ standard and POSIX (Section "Common Pitfalls > Pitfall 1") |
| EVENT-03 | Signal handlers push events instead of mutating global atomic flags | Signal handler transformation (Section "Architecture Patterns > Signal Handler Transformation") shows each handler reduced to a single `queue.push(Event{...})` call; no more `app_state.store()` or `term_resize()` calls from signal context |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ `std::atomic` | C++17+ (project uses C++23) | Lock-free head/tail indices for ring buffer | Already used throughout codebase; lock-free atomics are the only signal-safe synchronization primitive |
| C++ `std::variant` | C++17+ | Type-safe event discriminated union | Already planned for the project; zero-allocation; compiler-exhaustiveness checking |
| C++ `std::array` | C++11+ | Fixed-capacity ring buffer storage | No heap allocation; size known at compile time |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Google Test | v1.17.0 | Unit tests for event types and queue operations | Already in project; test push/pop, overflow behavior, FIFO ordering |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom SPSC ring buffer | `boost::lockfree::spsc_queue` | Boost adds a dependency; btop has a no-new-dependencies policy; our queue is ~60 lines |
| Custom SPSC ring buffer | `rigtorp/SPSCQueue` | External dependency; more features than needed; our use case is trivial (signal handler pushes tiny structs) |
| `std::variant` events | Raw enum + union | Less type-safe; no compiler exhaustiveness checking; variant is already the project's chosen pattern |
| Fixed-size ring buffer | `std::deque` or heap-allocated queue | Not signal-safe; heap allocation is forbidden in signal handlers |

## Architecture Patterns

### Event Type Design

Events must be **trivially copyable** to be safely written from a signal handler into the ring buffer. This means no `std::string`, no `std::vector`, no virtual functions.

```cpp
// src/btop_events.hpp
namespace event {
    struct TimerTick  {};                    // Main loop timer expired
    struct Resize     {};                    // SIGWINCH received
    struct Quit       { int exit_code; };    // SIGINT/SIGTERM
    struct Sleep      {};                    // SIGTSTP
    struct Resume     {};                    // SIGCONT
    struct Reload     {};                    // SIGUSR2
    struct ThreadError {};                   // Runner thread exception (msg in Global::exit_error_msg)
}

using AppEvent = std::variant<
    event::TimerTick,
    event::Resize,
    event::Quit,
    event::Sleep,
    event::Resume,
    event::Reload,
    event::ThreadError
>;

static_assert(std::is_trivially_copyable_v<AppEvent>,
    "AppEvent must be trivially copyable for signal-safe queue");
```

**Design decisions:**

1. **No `KeyInput` event in the queue.** Key input arrives via `pselect()` + `read()` in the main loop's `Input::poll()`. It is not delivered by signals. Phase 12 (Extract Transitions) will wrap key input into events at the main-loop level, but Phase 11 focuses on signal-to-main-loop communication only.

2. **No string data in events.** `ThreadError` does not carry the error message. The runner thread already writes `Global::exit_error_msg` with `memory_order_release` before pushing the event, and the main loop reads it with `memory_order_acquire`. This preserves the existing pattern established in Phase 10.

3. **`Quit` carries exit_code.** SIGINT produces `Quit{0}`, and a thread error path produces `ThreadError{}` (exit code 1 is implicit). This mirrors the existing `clean_quit(0)` vs `clean_quit(1)` distinction.

4. **Separate `Sleep` and `Resume` events.** Currently SIGTSTP sets `AppState::Sleeping` and SIGCONT calls `_resume()` directly. With events, both become queue pushes. The main loop handles sleep/resume sequencing.

5. **`TimerTick` is NOT pushed from a signal handler.** It is generated by the main loop's timer logic. It is included in the event variant for Phase 12 (event-driven main loop) but is not used in Phase 11's signal handlers. Its inclusion in the variant type now avoids a breaking type change later.

### Lock-Free SPSC Ring Buffer

The queue uses a classic SPSC ring buffer pattern: two atomic indices (`head` for producer, `tail` for consumer), a fixed-size array of event slots, and acquire-release memory ordering.

**Signal-safety argument:** The only operations performed by `push()` are:
1. `tail_.load(std::memory_order_acquire)` -- lock-free atomic load (signal-safe per C++17)
2. Array element assignment via `memcpy`-equivalent (trivially copyable type, signal-safe)
3. `head_.store(next, std::memory_order_release)` -- lock-free atomic store (signal-safe per C++17)

No heap allocation. No mutex. No `notify_one/notify_all` (which are NOT signal-safe).

```
Push (signal handler):                Pop (main loop):
  load tail (acquire)                   load head (acquire)
  if full -> drop event                 if empty -> return nullopt
  write slot[head]                      read slot[tail]
  store head (release)                  store tail (release)
                                        return event
```

**Capacity:** 32 slots. Signals arrive at human-interaction rates (SIGWINCH during terminal resize: maybe 10-20 per second burst; SIGINT: once). 32 is vastly oversized but costs only `32 * sizeof(AppEvent)` bytes (roughly 256 bytes). Power-of-two sizing enables bitwise modulo (`& (N-1)`) instead of integer division.

**Overflow policy:** Drop on full. If 32 events accumulate without the main loop processing them, the app is already in trouble (stuck or crashed). Dropping is safe because:
- Resize events are idempotent (multiple resizes collapse to one recalculation)
- Quit events: if the queue is full and we can't push quit, the app is stuck anyway
- The main loop processes events every iteration (~100ms max interval)

### Signal Handler Transformation

**Before (current):**
```cpp
static void _signal_handler(const int sig) {
    switch (sig) {
        case SIGINT:
            if (Runner::active) {
                Global::app_state.store(AppState::Quitting);
                Runner::stopping = true;
                Input::interrupt();
            } else {
                clean_quit(0);   // NOT signal-safe!
            }
            break;
        case SIGTSTP:
            if (Runner::active) {
                Global::app_state.store(AppState::Sleeping);
                Runner::stopping = true;
                Input::interrupt();
            } else {
                _sleep();        // NOT signal-safe!
            }
            break;
        case SIGCONT:
            _resume();           // NOT signal-safe!
            break;
        case SIGWINCH:
            term_resize();       // NOT signal-safe (when !Input::polling)!
            break;
        case SIGUSR2:
            Global::app_state.store(AppState::Reloading);
            Input::interrupt();
            break;
    }
}
```

**After (Phase 11):**
```cpp
static void _signal_handler(const int sig) {
    switch (sig) {
        case SIGINT:
            Global::event_queue.push(event::Quit{0});
            break;
        case SIGTSTP:
            Global::event_queue.push(event::Sleep{});
            break;
        case SIGCONT:
            Global::event_queue.push(event::Resume{});
            break;
        case SIGWINCH:
            Global::event_queue.push(event::Resize{});
            break;
        case SIGUSR2:
            Global::event_queue.push(event::Reload{});
            break;
    }
    // Wake up main loop if blocked in pselect()
    // kill(getpid(), SIGUSR1) is async-signal-safe
    Input::interrupt();
}
```

Key improvements:
1. **Every handler is trivially signal-safe.** Just one atomic push + one `kill()` call. Both are async-signal-safe.
2. **No more conditional branching on `Runner::active`.** The main loop handles the active/inactive distinction when processing the event.
3. **No more calling `clean_quit()`, `_sleep()`, `_resume()`, or `term_resize()` from signal context.** These are complex functions that are NOT async-signal-safe. The main loop calls them instead.
4. **SIGUSR1 handler remains unchanged** (no-op, used only to interrupt pselect).

### Main Loop Event Processing

The main loop drains the event queue at the top of each iteration. For Phase 11, the events are translated into `AppState` changes (preserving the existing if/else if structure). Phase 12 will replace the if/else if chain with `std::visit` dispatch.

```cpp
// At top of main loop iteration, BEFORE checking app_state:
while (auto ev = Global::event_queue.try_pop()) {
    std::visit([](const auto& e) { process_signal_event(e); }, *ev);
}

// Event processors (Phase 11 — translates events back to state changes):
void process_signal_event(const event::Quit& e) {
    if (Runner::active) {
        Global::app_state.store(AppState::Quitting);
        Runner::stopping = true;
    } else {
        clean_quit(e.exit_code);
    }
}

void process_signal_event(const event::Sleep&) {
    if (Runner::active) {
        Global::app_state.store(AppState::Sleeping);
        Runner::stopping = true;
    } else {
        _sleep();
    }
}

void process_signal_event(const event::Resume&) {
    _resume();
}

void process_signal_event(const event::Resize&) {
    term_resize();
}

void process_signal_event(const event::Reload&) {
    Global::app_state.store(AppState::Reloading);
}

// No-ops for events not yet used in Phase 11:
void process_signal_event(const event::TimerTick&) {}
void process_signal_event(const event::ThreadError&) {
    // ThreadError is still set via app_state.store(Error) from runner thread
    // This event type exists for Phase 12; in Phase 11 the runner thread
    // continues to use the direct atomic store pattern
}
```

**Priority preservation:** The existing priority ordering (Error > Quitting > Sleeping > Reloading > Resizing > Running) is preserved because the event processing translates to the same `app_state` stores, and the if/else if chain below continues to check in priority order. Events are processed FIFO, but since each event sets the app_state and the state check respects priority, a Quit arriving after a Resize will correctly take priority.

### ThreadError: Hybrid Approach

The runner thread's error path is special because it runs in a regular thread (not a signal handler), so it has no signal-safety constraints. In Phase 11, the runner thread continues to use the direct `app_state.store(AppState::Error, memory_order_release)` pattern established in Phase 10. The `event::ThreadError` struct exists in the variant for Phase 12's use but is not actively pushed in Phase 11.

This avoids changing the runner thread in Phase 11 (the runner thread is Phase 14's scope). The runner's error path remains:
```cpp
Global::exit_error_msg = "...";
Global::app_state.store(AppState::Error, std::memory_order_release);
Input::interrupt();
```

### Recommended File Structure

```
src/
├── btop_events.hpp     # NEW: Event types (variant) + EventQueue class
├── btop_state.hpp      # UNCHANGED: AppState enum (from Phase 10)
├── btop.cpp            # MODIFIED: Signal handlers push events; main loop drains queue
└── btop_shared.hpp     # MODIFIED: #include "btop_events.hpp" for extern queue declaration
```

### Anti-Patterns to Avoid

- **Putting `std::string` in event structs:** Makes the variant non-trivially-copyable. Cannot be used in a signal-safe queue. Store string data externally (Global::exit_error_msg pattern).
- **Using `std::mutex` or `std::condition_variable` in the queue:** Not async-signal-safe. Use only lock-free atomics.
- **Using `std::atomic::notify_one()` in signal handlers:** Not async-signal-safe per C++17. Use `kill(getpid(), SIGUSR1)` to wake the main loop.
- **Dynamically allocating in signal handlers:** `new`/`malloc` are not async-signal-safe. The ring buffer is statically sized.
- **Calling `std::visit` in signal handlers:** `std::visit` itself is fine (it's a pure function), but the visitor lambdas might call non-signal-safe functions. In Phase 11, signal handlers do not use `std::visit` -- they construct events directly.
- **Pushing TimerTick/KeyInput from signal handlers:** These events are NOT signal-originated. TimerTick is generated by the main loop's timer; KeyInput comes from `pselect()`/`read()`. Only signal-originated events go through the queue.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Lock-free queue | Mutex-based queue | SPSC ring buffer with atomic head/tail | Must be async-signal-safe; mutexes are not |
| Signal notification | Custom pipe/eventfd mechanism | Existing `Input::interrupt()` (SIGUSR1 -> pselect wakeup) | Already works, already integrated with pselect; no need to change |
| Event type discrimination | Manual tag + union | `std::variant` + `std::visit` | Type-safe, compiler-checked exhaustiveness, zero overhead |
| Thread-safe string passing for errors | String in event struct | Separate `Global::exit_error_msg` with memory_order_release/acquire | Keeps events trivially copyable; established pattern from Phase 10 |

**Key insight:** The event queue itself is simple (~60 lines). The complexity is in ensuring signal safety and correctly migrating the signal handler logic. Don't over-engineer the queue -- the throughput requirement is "a few events per second at most."

## Common Pitfalls

### Pitfall 1: Non-Signal-Safe Operations in Signal Handlers
**What goes wrong:** Calling `clean_quit()`, `_sleep()`, `_resume()`, or `term_resize()` from a signal handler. These functions allocate memory, use mutexes, and perform I/O. If the main thread is in the middle of a malloc when a signal arrives, calling malloc again from the handler causes deadlock or corruption.
**Why it happens:** The current code already does this (it's a pre-existing bug). Phase 11 fixes it by making all signal handlers trivially safe (just an atomic push + kill).
**How to avoid:** Signal handlers should ONLY: (1) push an event to the queue, and (2) call `Input::interrupt()`. Nothing else.
**Warning signs:** Rare hangs, corrupted output, or crashes on resize/quit that are hard to reproduce.

### Pitfall 2: Variant Not Trivially Copyable
**What goes wrong:** Adding a `std::string` or other non-trivially-copyable member to an event struct makes the entire `AppEvent` variant non-trivially-copyable. The `static_assert` catches this at compile time, but it's easy to accidentally introduce.
**Why it happens:** Natural instinct to put error messages or key strings inside event structs.
**How to avoid:** Keep all event structs as POD types. Use the `static_assert(std::is_trivially_copyable_v<AppEvent>)` guard. Store complex data externally.
**Warning signs:** Compile error on the static_assert.

### Pitfall 3: Missing Input::interrupt() After Push
**What goes wrong:** Signal handler pushes an event but doesn't call `Input::interrupt()`. The main loop is blocked in `pselect()` and doesn't wake up to process the event until the next timeout (up to 1000ms).
**Why it happens:** Forgetting that the main loop blocks on `pselect()` for input, not on the event queue.
**How to avoid:** Every signal handler that pushes an event MUST also call `Input::interrupt()` to wake the main loop. Consider making this part of the `push()` method (but note: `push()` must remain signal-safe, and `kill()` IS signal-safe).
**Warning signs:** Sluggish response to signals (1-second delay before resize or quit takes effect).

### Pitfall 4: Race Between Queue Drain and State Check
**What goes wrong:** Main loop drains the queue (sets `app_state` to `Quitting`), then later in the same iteration checks `app_state` -- but the state was already set to something else by another event processed after the quit event.
**Why it happens:** Multiple events in the queue processed in FIFO order; the last one wins if they all set `app_state`.
**How to avoid:** Process events in FIFO order but apply the priority rule: once `app_state` is set to a terminal state (Quitting, Error), stop processing further events. Use early-exit in the drain loop:
```cpp
while (auto ev = queue.try_pop()) {
    std::visit([](const auto& e) { process_signal_event(e); }, *ev);
    auto s = Global::app_state.load();
    if (s == AppState::Quitting or s == AppState::Error) break;
}
```
**Warning signs:** Quit signal being "ignored" if a resize event arrives immediately after.

### Pitfall 5: SIGCONT Handler Calling _resume() Directly
**What goes wrong:** If SIGCONT is handled in the signal handler (calling `_resume()` which calls `Term::init()` + `term_resize(true)`), it performs non-signal-safe operations. But if SIGCONT is queued like other events, the terminal may not be properly re-initialized before the main loop runs.
**Why it happens:** SIGCONT is special -- it's delivered when the process is resumed from a stopped state. The terminal must be re-initialized immediately because the main loop needs a working terminal to process events.
**How to avoid:** Queue the SIGCONT event like other signals. The main loop will process it and call `_resume()`. The slight delay (microseconds to milliseconds) between resume and terminal re-init is imperceptible to users. The process is already "running" when SIGCONT is delivered -- pselect will return with EINTR (or the signal handler's `Input::interrupt()` will wake it), so the main loop runs almost immediately.
**Warning signs:** Brief visual glitch on resume from suspend (acceptable, existed before too due to terminal state).

### Pitfall 6: Queue Overflow During Rapid Resize
**What goes wrong:** Rapid terminal resizing (e.g., dragging the window edge) generates many SIGWINCH signals in quick succession. If the queue fills up (32 slots), resize events are dropped.
**Why it happens:** Signal handler pushes faster than main loop processes.
**How to avoid:** 32 slots is generous (main loop processes in ~10ms, signals arrive at ~10-20Hz during resize). Even so, resize events are idempotent -- missing one just means the terminal gets resized on the next one. Additionally, consider deduplication: the main loop can collapse multiple consecutive Resize events into one.
**Warning signs:** None visible -- resize handling is naturally idempotent.

## Code Examples

### Complete EventQueue Implementation

```cpp
// src/btop_events.hpp
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

namespace event {
    struct TimerTick   {};                  // Main loop timer expired (Phase 12)
    struct Resize      {};                  // SIGWINCH
    struct Quit        { int exit_code; };  // SIGINT / SIGTERM
    struct Sleep       {};                  // SIGTSTP
    struct Resume      {};                  // SIGCONT
    struct Reload      {};                  // SIGUSR2
    struct ThreadError {};                  // Runner thread exception
}

using AppEvent = std::variant<
    event::TimerTick,
    event::Resize,
    event::Quit,
    event::Sleep,
    event::Resume,
    event::Reload,
    event::ThreadError
>;

static_assert(std::is_trivially_copyable_v<AppEvent>,
    "AppEvent must be trivially copyable for signal-safe queue operations");

/// Lock-free SPSC ring buffer, safe for use in signal handlers (push side).
/// Producer: signal handler context (async-signal-safe push).
/// Consumer: main loop thread (try_pop).
template<typename T, std::size_t Capacity>
    requires (Capacity > 0 && (Capacity & (Capacity - 1)) == 0)  // power of 2
class EventQueue {
    static_assert(std::is_trivially_copyable_v<T>,
        "EventQueue elements must be trivially copyable for signal safety");

    // Cache-line aligned to avoid false sharing between producer and consumer
    alignas(64) std::atomic<std::uint32_t> head_{0};   // written by producer
    alignas(64) std::atomic<std::uint32_t> tail_{0};   // written by consumer
    std::array<T, Capacity> slots_{};

    static constexpr std::uint32_t mask = Capacity - 1;

public:
    /// Push an event. Returns true if pushed, false if queue is full (event dropped).
    /// ASYNC-SIGNAL-SAFE: uses only lock-free atomic load/store + trivial copy.
    bool push(const T& item) noexcept {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto t = tail_.load(std::memory_order_acquire);
        if (h - t >= Capacity) return false;  // full
        slots_[h & mask] = item;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    /// Pop an event. Returns nullopt if queue is empty.
    /// NOT signal-safe (main thread only).
    std::optional<T> try_pop() noexcept {
        const auto t = tail_.load(std::memory_order_relaxed);
        const auto h = head_.load(std::memory_order_acquire);
        if (t == h) return std::nullopt;  // empty
        T item = slots_[t & mask];
        tail_.store(t + 1, std::memory_order_release);
        return item;
    }

    /// Check if queue has events without consuming them.
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }
};

namespace Global {
    /// Signal-to-main-loop event queue.
    /// Signal handlers push events; main loop pops and processes.
    inline constexpr std::size_t event_queue_capacity = 32;
    extern EventQueue<AppEvent, event_queue_capacity> event_queue;
}
```

### Signal Handler (After Migration)

```cpp
static void _signal_handler(const int sig) {
    switch (sig) {
        case SIGINT:
            Global::event_queue.push(event::Quit{0});
            break;
        case SIGTSTP:
            Global::event_queue.push(event::Sleep{});
            break;
        case SIGCONT:
            Global::event_queue.push(event::Resume{});
            break;
        case SIGWINCH:
            Global::event_queue.push(event::Resize{});
            break;
        case SIGUSR1:
            break;  // Input::poll interrupt, no event needed
        case SIGUSR2:
            Global::event_queue.push(event::Reload{});
            break;
    }
    // Always interrupt pselect to wake main loop (kill is async-signal-safe)
    if (sig != SIGUSR1) {
        Input::interrupt();
    }
}
```

### Main Loop Event Drain

```cpp
// Top of main loop, before existing state checks:
while (auto ev = Global::event_queue.try_pop()) {
    std::visit([](const auto& e) { process_signal_event(e); }, *ev);
    // Early exit on terminal states to preserve priority semantics
    auto s = Global::app_state.load(std::memory_order_acquire);
    if (s == AppState::Quitting or s == AppState::Error) break;
}
// ... existing if/else if chain on app_state continues below ...
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `volatile sig_atomic_t` flags | `std::atomic` with lock-free guarantee | C++11/C++17 clarifications | Signal-safe atomic operations without volatile |
| Self-pipe trick (write byte to pipe from handler) | Lock-free atomic queue + signal to interrupt pselect | Modern C++ practice | No pipe fd overhead; type-safe events instead of raw bytes |
| Signal handlers do complex work | Signal handlers only set flags/push events | Always best practice, but often violated | Eliminates UB from non-signal-safe calls |
| Manual if/else chain for multiple signal flags | Event queue with typed events | Elm Architecture / message-passing pattern | Decoupled, testable, extensible |

**Current btop has a pre-existing signal-safety violation:** `_signal_handler` for SIGWINCH calls `term_resize()`, which (when `Input::polling` is false) calls `atomic_lock`, `Term::refresh()`, and other non-signal-safe functions. Phase 11 fixes this by making all signal handlers trivially safe.

## Open Questions

1. **Should `Input::interrupt()` be called inside `push()` automatically?**
   - What we know: Every signal handler needs to call `Input::interrupt()` after pushing to wake the pselect-blocked main loop. Making it automatic would prevent forgetting it.
   - What's unclear: `push()` is supposed to be purely a data structure operation. Coupling it with signal delivery is a design smell. Also, `Input::interrupt()` calls `kill(getpid(), SIGUSR1)` which is signal-safe, so it could technically go in `push()`.
   - Recommendation: Keep them separate. The `push()` method is a general-purpose SPSC queue; `Input::interrupt()` is btop-specific notification. The signal handler is the right place to call both. This also keeps the queue testable without side effects.

2. **Should the runner thread use the event queue for ThreadError?**
   - What we know: The runner thread currently sets `Global::exit_error_msg` then `app_state.store(Error, release)` then `Input::interrupt()`. This works and is Phase 10's established pattern.
   - What's unclear: Whether Phase 11 should migrate this to use `event_queue.push(ThreadError{})` for consistency.
   - Recommendation: Leave the runner thread's error path unchanged in Phase 11. The runner thread is Phase 14's scope. The `event::ThreadError` type exists in the variant so Phase 12/14 can use it, but Phase 11 should not touch runner thread code.

3. **Event deduplication for Resize?**
   - What we know: Rapid window resizing can generate many SIGWINCH signals. Multiple Resize events in the queue are redundant -- only one recalculation is needed.
   - What's unclear: Whether deduplication in the main loop is worth the complexity.
   - Recommendation: Not needed for Phase 11. The main loop already handles resize idempotently (it calls `term_resize()` which has its own guard). If 5 Resize events are in the queue, processing each one just calls `term_resize()` which does nothing if the terminal hasn't actually changed since the last check. Deduplication is a premature optimization.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 (via FetchContent) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R Event --output-on-failure` |
| Full suite command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| EVENT-01 | AppEvent variant contains all 7 event types | unit | `ctest --test-dir build/tests -R Event --output-on-failure` | Wave 0 |
| EVENT-01 | AppEvent is trivially copyable | unit (static_assert + test) | `ctest --test-dir build/tests -R Event --output-on-failure` | Wave 0 |
| EVENT-01 | Each event struct is constructible with expected fields | unit | `ctest --test-dir build/tests -R Event --output-on-failure` | Wave 0 |
| EVENT-02 | EventQueue push/pop returns correct events in FIFO order | unit | `ctest --test-dir build/tests -R EventQueue --output-on-failure` | Wave 0 |
| EVENT-02 | EventQueue reports full when capacity exceeded | unit | `ctest --test-dir build/tests -R EventQueue --output-on-failure` | Wave 0 |
| EVENT-02 | EventQueue try_pop returns nullopt when empty | unit | `ctest --test-dir build/tests -R EventQueue --output-on-failure` | Wave 0 |
| EVENT-02 | EventQueue head/tail atomics are lock-free | unit | `ctest --test-dir build/tests -R EventQueue --output-on-failure` | Wave 0 |
| EVENT-03 | Signal handlers push events (verified by build + behavioral equivalence) | integration (manual) | Build succeeds + btop runs identically | manual |
| EVENT-03 | Resize signal queues Resize event and triggers state change | integration (manual) | Resize terminal while running btop | manual |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure`
- **Per wave merge:** Full suite + manual build on macOS (primary dev platform)
- **Phase gate:** Full suite green + manual signal handling verification (resize, Ctrl+C, Ctrl+Z/fg, kill -USR2)

### Wave 0 Gaps
- [ ] `tests/test_events.cpp` -- covers EVENT-01, EVENT-02 (event types, variant properties, queue push/pop/overflow/empty/FIFO)
- [ ] Update `tests/CMakeLists.txt` -- add `test_events.cpp` to `btop_test` sources

*(Test infrastructure (Google Test, CMake integration) already exists -- only the test file is missing.)*

## Sources

### Primary (HIGH confidence)
- Source code analysis of `src/btop.cpp` lines 294-330 (current signal handler `_signal_handler`)
- Source code analysis of `src/btop.cpp` lines 134-211 (`term_resize`, `clean_quit`, `_sleep`, `_resume`)
- Source code analysis of `src/btop.cpp` lines 1300-1393 (main loop)
- Source code analysis of `src/btop.cpp` lines 466-770 (Runner thread `_runner` function)
- Source code analysis of `src/btop_input.cpp` lines 99-125 (`Input::poll` using pselect)
- Source code analysis of `src/btop_input.cpp` lines 210-212 (`Input::interrupt` using kill/SIGUSR1)
- Source code analysis of `src/btop_state.hpp` (AppState enum from Phase 10)
- Phase 10 research and verification (`.planning/phases/10-name-states/10-RESEARCH.md`, `10-VERIFICATION.md`)
- `.planning/research/AUTOMATA-ARCHITECTURE.md` (domain research, event queue design)
- `.planning/REQUIREMENTS.md` (EVENT-01, EVENT-02, EVENT-03 definitions)

### Secondary (MEDIUM confidence)
- [cppreference: std::signal](https://en.cppreference.com/w/cpp/utility/program/signal) - C++17 rules for signal handler-safe operations; lock-free atomics explicitly allowed
- [Linux man page: signal-safety(7)](https://man7.org/linux/man-pages/man7/signal-safety.7.html) - POSIX async-signal-safe function list; `write()` and `kill()` confirmed safe
- [SEI CERT C: SIG30-C](https://wiki.sei.cmu.edu/confluence/display/c/SIG30-C.+Call+only+asynchronous-safe+functions+within+signal+handlers) - Signal handler safety guidelines
- [Rigtorp SPSCQueue](https://github.com/rigtorp/SPSCQueue) - Reference implementation of lock-free SPSC ring buffer pattern
- [Optimizing a Ring Buffer for Throughput (Erik Rigtorp)](https://rigtorp.se/ringbuffer/) - Cache-line alignment, false sharing prevention patterns
- [Thomas Trapp: Wrangling POSIX Signals in Multithreaded C++](https://thomastrapp.com/blog/signal-handlers-for-multithreaded-cpp/) - Signal handling patterns in multithreaded C++

### Tertiary (LOW confidence)
- None. All findings are from direct source code analysis and verified documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Using only C++ standard library features already present in the codebase (atomic, variant, array)
- Architecture: HIGH - SPSC ring buffer is a well-understood pattern; signal-safety rules are clearly defined in the C++ standard; the codebase already uses the atomic + pselect + SIGUSR1 pattern that the queue builds on
- Pitfalls: HIGH - All pitfalls identified from actual source code analysis (signal-unsafe calls in current handlers, variant triviality requirements, pselect notification mechanism)

**Research date:** 2026-02-28
**Valid until:** 2026-03-28 (stable -- no external dependencies, pure internal refactoring using C++ standard library)
