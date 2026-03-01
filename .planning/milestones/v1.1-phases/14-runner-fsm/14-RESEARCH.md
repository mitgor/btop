# Phase 14: Runner FSM - Research

**Researched:** 2026-03-01
**Domain:** Replacing Runner thread's atomic<bool> flags with an independent std::variant FSM, typed main-to-runner events
**Confidence:** HIGH

## Summary

Phase 14 extracts the Runner thread's coordination logic from a set of scattered `atomic<bool>` flags into an independent finite state machine using `std::variant`, following the same pattern established in Phases 10-13 for the App FSM. The Runner thread currently uses five `atomic<bool>` flags (`active`, `stopping`, `waiting`, `redraw`, `coreNum_reset`) plus a `bool pause_output` and a `std::mutex` (`mtx`) combined with a `std::binary_semaphore` (`do_work`) to coordinate between the main thread and the runner thread. Two of these flags (`waiting` and `reading`) are dead code (declared but never referenced after definition). The remaining flags encode an implicit state machine: the runner is either idle (waiting on semaphore), actively collecting/drawing (active=true), or being asked to stop (stopping=true).

The core challenge is that the Runner FSM is inherently cross-thread: the main thread writes to `Runner::stopping` and calls `Runner::run()` / `Runner::stop()`, while the runner thread reads `stopping`, sets `active` via `atomic_lock`, and reads `redraw`. Additionally, `Runner::stopping` is read from collector code (5 platform-specific files) and draw code (btop_draw.cpp) as a cooperative cancellation check. The FSM design must preserve these cross-thread read patterns, likely using the same "shadow atomic tag + thread-local variant" pattern from Phase 13, but with the runner thread owning its variant state.

The second challenge is replacing the main-to-runner communication. Currently, the main thread mutates `Runner::stopping`, `Runner::redraw`, and `Runner::pause_output` directly. Phase 14 replaces these with typed events sent to the runner, preserving the binary semaphore wake-up pattern for zero-regression latency.

**Primary recommendation:** Define `RunnerStateVar = std::variant<runner::Idle, runner::Collecting, runner::Drawing, runner::Stopping>` owned by the runner thread. Keep a shadow `atomic<RunnerStateTag>` for cross-thread queries (replacing `Runner::active`). Replace main-to-runner flag mutation with a dedicated runner event queue or typed command struct. Preserve the `binary_semaphore` wake-up. Leave `Runner::stopping` reads in collectors/draw as reads of the shadow tag (`== RunnerStateTag::Stopping`) for minimal cross-platform diff.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| RUNNER-01 | Runner thread states defined as std::variant (Idle, Collecting, Drawing, Stopping) | Architecture pattern "Runner State Variant" defines each state struct with per-state data. Source code analysis identifies exactly four logical states from the existing flag combinations. |
| RUNNER-02 | Runner atomic<bool> flags (active, stopping, waiting, redraw) replaced by FSM states | Flag taxonomy analysis maps each flag to either an FSM state (active->Collecting/Drawing, stopping->Stopping, waiting->Idle) or a runner event (redraw). Dead flags (waiting, reading) are removed. coreNum_reset remains as-is (Linux-specific collector signal, not runner state). |
| RUNNER-03 | Main thread -> runner communication via typed events, not flag mutation | Architecture pattern "Runner Command Channel" defines RunnerCmd variant with Start, Stop, Redraw command types. Main thread pushes commands; runner thread pops and transitions. |
| RUNNER-04 | Binary semaphore wake-up pattern preserved for performance | Architecture pattern "Semaphore Preservation" documents that the binary_semaphore is kept as the wake mechanism, with typed commands replacing the implicit "what should I do" flag checks. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ `std::variant` | C++17+ (project uses C++23) | Type-safe runner state union (Idle/Collecting/Drawing/Stopping) | Already used for AppStateVar and AppEvent; extending to runner state is natural |
| C++ `std::binary_semaphore` | C++20 | Runner thread wake-up signal (existing, preserved) | Already in use; zero-cost wake latency, signal-safe release |
| C++ `std::atomic` | C++11+ | Shadow tag for cross-thread RunnerState queries | Already used for AppStateTag; same pattern applied to runner |
| C++ `std::mutex` | C++11+ | Runner thread lifetime guard (existing, preserved) | Already in use; protects thread loop vs clean_quit join |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Google Test | v1.17.0 | Unit tests for runner state transitions | Already in project; new test_runner_state.cpp for runner FSM |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Shadow atomic tag for cross-thread reads | Mutex-protect the variant | Mutex on every `Runner::stopping` check in collectors (6 files, hot path) adds unacceptable contention |
| Dedicated runner event queue | Overload the existing AppEvent queue | AppEvent queue is SPSC signal->main; runner needs main->runner channel; mixing them violates the SPSC contract |
| Replace binary_semaphore with condition_variable | Allows richer signaling | binary_semaphore is simpler, already works, no benefit to changing |
| Single shared FSM for App+Runner | Keep orthogonal FSMs | Orthogonal avoids 6x4=24 state explosion; decision from STATE.md |

## Architecture Patterns

### Runner Flag Taxonomy (Current State)

Complete enumeration of all Runner namespace members and their cross-thread access patterns:

| Flag | Type | Defined | Written By | Read By | Purpose | FSM Mapping |
|------|------|---------|------------|---------|---------|-------------|
| `active` | `atomic<bool>` | btop.cpp:414 | Runner thread (via atomic_lock) | Main thread, Input, Menu, Config, Draw | "Runner is currently doing work" | State: Collecting or Drawing |
| `stopping` | `atomic<bool>` | btop.cpp:415 | Main thread (run/stop/on_exit) | Runner thread, ALL collectors (5 files), Draw (5 checks) | "Runner should abandon current work" | State: Stopping |
| `waiting` | `atomic<bool>` | btop.cpp:416 | **NEVER WRITTEN** | **NEVER READ** | Dead code | Remove |
| `reading` | `atomic<bool>` | btop_shared.hpp:412 | **NEVER DEFINED** | **NEVER READ** | Dead code (extern only) | Remove |
| `redraw` | `atomic<bool>` | btop.cpp:417 | Draw::calcSizes (btop_draw.cpp:2763) | Runner thread (btop.cpp:720) | "Next frame should force full redraw" | Runner event: ForceRedraw |
| `coreNum_reset` | `atomic<bool>` | btop.cpp:418 | Linux collector (btop_collect.cpp:1180) | Runner thread (btop.cpp:602), Linux collector init (btop_collect.cpp:333) | "CPU core count changed, trigger resize" | Keep as-is (Linux-specific collector signal, not runner state) |
| `pause_output` | `bool` (non-atomic) | btop.cpp:451 | Menu (btop_menu.cpp:1485,1824,1854,1864), Draw (btop_draw.cpp:2762) | Runner thread (btop.cpp:613,637,659,699,727) | "Suppress drawing output (menu overlay active)" | Runner conf field (already in runner_conf partially) |
| `do_work` | `binary_semaphore` | btop.cpp:427 | Main thread (thread_trigger) | Runner thread (thread_wait/acquire) | "Wake runner to do work" | Preserve as wake mechanism |
| `mtx` | `std::mutex` | btop.cpp:453 | Runner thread (lock_guard) | Main thread (Runner::stop try_lock) | Thread alive guard | Preserve |

### Runner State Variant

The runner thread has four logical states derived from flag analysis:

```cpp
namespace runner {
    struct Idle {};       // Waiting on semaphore (was: active=false, stopping=false)

    struct Collecting {   // Performing data collection (was: active=true in collect phase)
        runner_conf conf; // The configuration for this collection cycle
    };

    struct Drawing {      // Rendering output to terminal (was: active=true in draw phase)
        bool force_redraw;
    };

    struct Stopping {};   // Cooperative cancellation in progress (was: stopping=true)
}

using RunnerStateVar = std::variant<
    runner::Idle,
    runner::Collecting,
    runner::Drawing,
    runner::Stopping
>;
```

**State data rationale:**

| State | Data | Source | Why |
|-------|------|--------|-----|
| Idle | none | Blocked on semaphore | No work in progress |
| Collecting | `runner_conf` | Currently `Runner::current_conf` | Collection needs boxes, no_update, force_redraw |
| Drawing | `force_redraw` | Currently from `redraw` flag + `conf.force_redraw` | Controls screen_buffer full vs diff emit |
| Stopping | none | Just a state tag | Collectors check tag to bail out early |

### Shadow Atomic Tag for Cross-Thread Reads

The same dual-representation pattern from Phase 13 applies here:

```cpp
enum class RunnerStateTag : std::uint8_t {
    Idle      = 0,
    Collecting = 1,
    Drawing   = 2,
    Stopping  = 3,
};

// Runner thread owns the variant (authoritative)
// Both threads can read the shadow tag
extern std::atomic<RunnerStateTag> runner_state_tag;  // replaces Runner::active + Runner::stopping
```

**Cross-thread read replacement map:**

| Current Code | Current Check | New Code |
|-------------|---------------|----------|
| `Runner::active` (true = busy) | `atomic_wait(Runner::active)` in Input/Menu/Config | `atomic_wait` on `runner_state_tag != Idle` |
| `Runner::active` (set_active) | `active.store(false); active.notify_all()` | `runner_state_tag.store(Idle); runner_state_tag.notify_all()` |
| `Runner::stopping` in collectors | `if (Runner::stopping) return` | `if (runner_state_tag.load() == RunnerStateTag::Stopping) return` |
| `Runner::stopping` in draw | `if (Runner::stopping) return ""` | `if (runner_state_tag.load() == RunnerStateTag::Stopping) return ""` |
| `!Runner::active` in main loop | `if (not Runner::active) Config::unlock()` | `if (runner_state_tag.load() == RunnerStateTag::Idle)` |

### Runner Command Channel (Main -> Runner Communication)

Instead of the main thread mutating `Runner::stopping`, `Runner::redraw`, and `Runner::pause_output` directly, it sends typed commands:

```cpp
namespace runner_cmd {
    struct Start {
        runner_conf conf;  // What to collect and draw
    };
    struct Stop {};        // Cooperative cancellation request
    struct ForceRedraw {}; // Next frame should force full redraw
}

using RunnerCmd = std::variant<
    runner_cmd::Start,
    runner_cmd::Stop,
    runner_cmd::ForceRedraw
>;
```

**Command delivery mechanism:** The simplest correct approach is a single-slot atomic command buffer (not a queue), since:
1. The main thread always waits for the runner to complete before sending a new Start command
2. Stop preempts any pending Start
3. ForceRedraw is coalesced (multiple redraws = one redraw)

However, looking at the actual interaction pattern more carefully:

- `Runner::run()` is called from the main thread, sets up `current_conf`, then calls `thread_trigger()` (semaphore release)
- The runner thread calls `thread_wait()` (semaphore acquire), then reads `current_conf`
- This is already effectively a single-slot command: `current_conf` IS the command payload

**Recommended approach:** Replace `current_conf` + flag mutation with a single `std::optional<RunnerCmd>` command slot (or keep `runner_conf` as the Start payload). The binary semaphore remains the wake signal. On `Runner::run()`, the main thread writes the command and releases the semaphore. The runner thread acquires the semaphore, reads the command, and transitions states accordingly.

For `Runner::stop()`, the main thread stores a Stop command and the runner checks `stopping` state at cancellation checkpoints.

### Semaphore Preservation (RUNNER-04)

The binary semaphore pattern MUST be preserved exactly:

```
Main thread                         Runner thread
-----------                         -------------
                                    do_work.acquire()  // blocks here (Idle state)
Runner::run("all"):
  write current_conf
  do_work.release()  ------------>  wakes up
                                    read current_conf
                                    state = Collecting
                                    ... collect data ...
                                    state = Drawing
                                    ... draw output ...
                                    state = Idle
                                    do_work.acquire()  // blocks again
```

The semaphore provides near-instant wake-up (microsecond-level) compared to polling. This is the critical performance property that RUNNER-04 protects.

### Runner Thread Loop Restructured

```cpp
static void* _runner(void*) {
    // Block signals (same as current)
    sigemptyset(&mask);
    sigaddset(&mask, SIGWINCH);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    std::lock_guard lock{mtx};
    RunnerStateVar runner_var = runner::Idle{};

    while (Global::app_state.load() != AppStateTag::Quitting) {
        // Idle: wait for work
        runner_state_tag.store(RunnerStateTag::Idle);
        runner_state_tag.notify_all();
        thread_wait();  // binary_semaphore.acquire()

        // Check for active lock timeout (same as current)
        // ... existing timeout logic ...

        if (runner_state_tag.load() == RunnerStateTag::Stopping
            or Global::app_state.load() == AppStateTag::Resizing) {
            sleep_ms(1);
            continue;
        }

        // Transition to Collecting
        runner_var = runner::Collecting{current_conf};
        runner_state_tag.store(RunnerStateTag::Collecting);
        runner_state_tag.notify_all();

        // ... collection phase ...
        // Cancellation checks: if (runner_state_tag == Stopping) continue;

        // Transition to Drawing
        runner_var = runner::Drawing{redraw_needed};
        runner_state_tag.store(RunnerStateTag::Drawing);

        // ... drawing phase ...

        // Back to Idle (loop continues)
    }
    return {};
}
```

### Recommended File Structure

```
src/
+-- btop_state.hpp      # ADD: RunnerStateTag enum, runner:: state structs, RunnerStateVar
|                        #   Follows same pattern as AppStateTag/AppStateVar
+-- btop_shared.hpp      # MODIFY: Replace Runner:: extern atomic<bool> declarations
|                        #   Remove: active, stopping, waiting, reading
|                        #   Add: extern atomic<RunnerStateTag> runner_state_tag
|                        #   Keep: run(), stop() function signatures
+-- btop.cpp             # MODIFY: Runner namespace implementation
|                        #   Replace atomic<bool> flags with variant + tag
|                        #   Restructure _runner() loop with explicit states
|                        #   Update run()/stop() to use typed commands
|                        #   Update on_exit() to use runner_state_tag
+-- btop_draw.cpp        # MODIFY: Runner::stopping -> runner_state_tag == Stopping (5 sites)
+-- btop_input.cpp       # MODIFY: Runner::active waits -> runner_state_tag waits (8 sites)
+-- btop_menu.cpp         # MODIFY: Runner::active waits (2 sites), pause_output (4 sites)
+-- btop_config.cpp       # MODIFY: Runner::active wait (1 site)
+-- osx/btop_collect.cpp  # MODIFY: Runner::stopping -> runner_state_tag (3 sites)
+-- linux/btop_collect.cpp # MODIFY: Runner::stopping -> runner_state_tag (6 sites)
+-- freebsd/btop_collect.cpp # MODIFY: Runner::stopping -> runner_state_tag (2 sites)
+-- openbsd/btop_collect.cpp # MODIFY: Runner::stopping -> runner_state_tag (2 sites)
+-- netbsd/btop_collect.cpp  # MODIFY: Runner::stopping -> runner_state_tag (2 sites)
+-- btop_tools.hpp        # MODIFY: atomic_wait/atomic_wait_for may need RunnerStateTag overloads
tests/
+-- test_runner_state.cpp # NEW: Unit tests for runner state transitions
```

### Anti-Patterns to Avoid

- **Making the runner variant visible to the main thread:** The runner thread owns its variant. The main thread communicates via the shadow tag (for queries) and the command channel (for instructions). No shared mutable variant.
- **Removing the mutex:** The `std::mutex` in `Runner::stop()` (try_lock pattern) serves as a thread-alive check. It must be preserved even though the FSM handles state transitions.
- **Using the AppEvent queue for runner commands:** The AppEvent queue is SPSC (signal handler -> main thread). Runner commands go main thread -> runner thread. These are different channels with different safety requirements.
- **Merging Runner states into App states:** The orthogonal FSM decision (from STATE.md) keeps App and Runner FSMs independent. Merging creates a 6x4=24 state explosion.
- **Removing coreNum_reset:** This flag is set by the Linux CPU collector when core count changes. It is not runner state -- it is a collector-to-runner signal. Keep as `atomic<bool>`.
- **Making pause_output a runner state:** `pause_output` is a rendering modifier (suppress drawing when menu overlay is active), not a runner lifecycle state. It should be part of `runner_conf` (already partially is via `background_update`) or a separate flag. It is set by Menu code from the main thread while the runner is active, so it needs atomic access.
- **Complex runner event queue:** The main-to-runner communication is inherently single-producer single-consumer with at most one outstanding command. A full SPSC queue is overkill. Use the existing `current_conf` + semaphore pattern with the conf struct becoming the typed command payload.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Cross-thread state query | Mutex-protect variant reads | Shadow `atomic<RunnerStateTag>` | Same pattern as Phase 13; proven in App FSM |
| Runner wake-up mechanism | Polling loop or condition_variable | Existing `binary_semaphore` (preserve) | Already correct, minimal latency |
| Cooperative cancellation | Custom cancellation token | `RunnerStateTag::Stopping` checked at safe points | Collector/draw code already has check points; just change the flag name |
| Thread-alive check | Custom flag | Existing `std::mutex` try_lock pattern | Already works; Runner::stop() depends on it |

**Key insight:** The Runner FSM is simpler than the App FSM because the runner thread has a linear lifecycle within each work cycle: Idle -> Collecting -> Drawing -> Idle (or Stopping at any point). There are no branching transitions like Reloading -> Resizing -> Running. The complexity is in the cross-thread communication, not the state machine itself.

## Common Pitfalls

### Pitfall 1: Breaking Cooperative Cancellation in Collectors
**What goes wrong:** `Runner::stopping` is read in 24 locations across 7 files. If the replacement (`runner_state_tag == Stopping`) has different semantics (e.g., memory ordering), collectors may not bail out correctly, leading to hangs during quit/resize.
**Why it happens:** `Runner::stopping` is currently `atomic<bool>` with default (sequentially consistent) loads. The shadow tag uses relaxed loads in some patterns.
**How to avoid:** Use `memory_order_acquire` for all `runner_state_tag` reads at cancellation check points (matching the current seq_cst default). Provide a helper function: `inline bool runner_stopping() { return runner_state_tag.load(std::memory_order_acquire) == RunnerStateTag::Stopping; }`.
**Warning signs:** btop hangs on Ctrl+C, resize takes >5 seconds, `kill -USR2` causes freeze.

### Pitfall 2: Race Between Stop and Active Lock
**What goes wrong:** The `atomic_lock` pattern (RAII sets `active=true`, destructor sets `active=false`) is used to signal "runner is busy." If replaced improperly, the main thread may send a new command while the runner is still processing the previous one.
**Why it happens:** The `atomic_wait_for(active, true, 5000)` pattern in `Runner::run()` waits for the runner to finish before sending new work. If the shadow tag doesn't transition to Idle before the main thread checks, a race occurs.
**How to avoid:** The runner thread must store `RunnerStateTag::Idle` and `notify_all()` at the exact point where `atomic_lock` destructor currently runs (end of work cycle). The `Runner::run()` function waits on the tag, not a separate `active` flag.
**Warning signs:** "Stall in Runner thread, restarting!" log messages. Runner thread re-creation.

### Pitfall 3: pause_output Thread Safety
**What goes wrong:** `Runner::pause_output` is a non-atomic `bool` read by the runner thread and written by Menu code (main thread). Currently this is a benign data race (worst case: one frame with/without overlay). Making it part of the FSM could introduce synchronization overhead on a hot path.
**Why it happens:** The current code accepts the benign race for simplicity.
**How to avoid:** Keep `pause_output` as a `bool` or make it `atomic<bool>`. Do NOT make it a runner state -- it is an output modifier, not a lifecycle state. Include it in `runner_conf` (it is already partially there via `background_update`). The runner reads it only when building output, which is naturally after the main thread has set it.
**Warning signs:** Menu overlay flickers. Blank screen when opening menu.

### Pitfall 4: Deadlock in Runner::stop()
**What goes wrong:** `Runner::stop()` uses a complex dance: set `stopping=true`, try_lock mutex, check `active`, wait, trigger, wait. If the FSM transition logic changes this sequence, the runner thread may be waiting on the semaphore while `stop()` waits on the tag, causing deadlock.
**Why it happens:** The current `stop()` function uses the mutex try_lock to determine if the runner is alive (locked = running loop, unlocked = thread died). This logic must be preserved.
**How to avoid:** Keep the `stop()` function's try_lock pattern. Replace `stopping = true` with `runner_state_tag.store(Stopping)` at the beginning. Replace `atomic_wait_for(active, true, 5000)` with a wait on the tag. The semaphore release (`thread_trigger()`) in `stop()` wakes the runner if it is blocked on `acquire()`.
**Warning signs:** btop hangs indefinitely on exit. `clean_quit()` never completes.

### Pitfall 5: Large Cross-Platform Diff
**What goes wrong:** Changing `Runner::stopping` to `runner_state_tag == Stopping` across 5 platform-specific collector files creates a large diff that is hard to review and test.
**Why it happens:** The stopping check is in every collector's `collect()` function as an early-exit guard.
**How to avoid:** Provide a free function `Runner::is_stopping()` that wraps the tag check. Collectors call `Runner::is_stopping()` instead of `Runner::stopping`. This minimizes the diff to one symbol change per call site and centralizes the implementation.
**Warning signs:** Build failures on BSD platforms that were not tested.

## Code Examples

### Runner State Definitions (btop_state.hpp additions)

```cpp
namespace Global {
    /// Runner thread lifecycle states.
    enum class RunnerStateTag : std::uint8_t {
        Idle       = 0,  ///< Waiting for work (semaphore blocked)
        Collecting = 1,  ///< Data collection in progress
        Drawing    = 2,  ///< Rendering output
        Stopping   = 3,  ///< Cooperative cancellation
    };

    extern std::atomic<RunnerStateTag> runner_state_tag;

    constexpr std::string_view to_string(RunnerStateTag s) noexcept {
        switch (s) {
            case RunnerStateTag::Idle:       return "Idle";
            case RunnerStateTag::Collecting: return "Collecting";
            case RunnerStateTag::Drawing:    return "Drawing";
            case RunnerStateTag::Stopping:   return "Stopping";
        }
        return "Unknown";
    }
}

namespace runner {
    struct Idle {};

    struct Collecting {
        // Per-cycle collection configuration (previously Runner::current_conf)
    };

    struct Drawing {
        bool force_redraw;
    };

    struct Stopping {};
}

using RunnerStateVar = std::variant<
    runner::Idle,
    runner::Collecting,
    runner::Drawing,
    runner::Stopping
>;
```

### Compatibility Wrapper for Collectors (btop_shared.hpp)

```cpp
namespace Runner {
    // Cooperative cancellation check -- replaces direct Runner::stopping reads
    inline bool is_stopping() noexcept {
        return Global::runner_state_tag.load(std::memory_order_acquire)
            == Global::RunnerStateTag::Stopping;
    }

    // Active check -- replaces direct Runner::active reads
    inline bool is_active() noexcept {
        auto tag = Global::runner_state_tag.load(std::memory_order_acquire);
        return tag == Global::RunnerStateTag::Collecting
            || tag == Global::RunnerStateTag::Drawing;
    }

    // Wait for runner to become idle (replaces atomic_wait(Runner::active))
    void wait_idle() noexcept;  // wraps atomic_wait on runner_state_tag

    // Keep existing interface:
    void run(const string& box = "", bool no_update = false, bool force_redraw = false);
    void stop();
}
```

### Updated Runner::run() (btop.cpp)

```cpp
void Runner::run(const string& box, bool no_update, bool force_redraw) {
    // Wait for runner to be idle (replaces atomic_wait_for(active, true, 5000))
    wait_for_idle(5000);

    if (is_active()) {
        Logger::error("Stall in Runner thread, restarting!");
        runner_state_tag.store(RunnerStateTag::Idle, std::memory_order_release);
        runner_state_tag.notify_all();
        pthread_cancel(Runner::runner_id);
        // ... restart logic same as current ...
    }

    if (runner_state_tag.load() == RunnerStateTag::Stopping
        or Global::app_state.load() == AppStateTag::Resizing) return;

    // Handle overlay/clock shortcuts (same as current)
    if (box == "overlay" or box == "clock") {
        // ... direct write_stdout, same as current ...
        return;
    }

    // Set up runner configuration (command payload)
    Config::unlock();
    Config::lock();
    current_conf = { /* same as current */ };

    // Wake runner thread
    thread_trigger();  // binary_semaphore.release()
    wait_for_idle(10); // brief wait for runner to start (same as current)
}
```

### Updated Runner Thread Loop (btop.cpp)

```cpp
static void* _runner(void*) {
    sigemptyset(&mask);
    sigaddset(&mask, SIGWINCH);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    std::lock_guard lock{mtx};
    RunnerStateVar runner_var = runner::Idle{};

    while (Global::app_state.load() != AppStateTag::Quitting) {
        // === IDLE STATE ===
        runner_var = runner::Idle{};
        runner_state_tag.store(RunnerStateTag::Idle, std::memory_order_release);
        runner_state_tag.notify_all();

        thread_wait();  // binary_semaphore.acquire() -- blocks until work arrives

        // Timeout check (same as current active lock check)
        // ...

        if (runner_state_tag.load() == RunnerStateTag::Stopping
            or Global::app_state.load() == AppStateTag::Resizing) {
            sleep_ms(1);
            continue;
        }

        // === COLLECTING STATE ===
        auto& conf = current_conf;
        runner_var = runner::Collecting{};
        runner_state_tag.store(RunnerStateTag::Collecting, std::memory_order_release);
        runner_state_tag.notify_all();

        // SUID privilege escalation (same as current)
        gain_priv powers{};

        output.clear();

        try {
            // ... collection code (same as current) ...
            // Cancellation checks use Runner::is_stopping()
        }
        catch (const std::exception& e) {
            // ... error handling (same as current) ...
        }

        if (Runner::is_stopping()) continue;

        // === DRAWING STATE ===
        runner_var = runner::Drawing{redraw_needed};
        runner_state_tag.store(RunnerStateTag::Drawing, std::memory_order_release);

        // ... rendering code (same as current) ...
    }

    return {};
}
```

### on_exit Updates for Runner FSM (btop.cpp)

```cpp
// Phase 14: on_exit uses runner_state_tag instead of Runner::active/stopping
static void on_exit(const state::Running&, const state::Quitting&) {
    if (Runner::is_active()) {
        Global::runner_state_tag.store(Global::RunnerStateTag::Stopping);
    }
}

static void on_exit(const state::Running&, const state::Sleeping&) {
    if (Runner::is_active()) {
        Global::runner_state_tag.store(Global::RunnerStateTag::Stopping);
    }
}
```

### Collector Cancellation Check (all btop_collect.cpp files)

```cpp
// Before (current):
if (Runner::stopping or (no_update and not current_cpu.cpu_percent[...].empty()))
    return current_cpu;

// After (Phase 14):
if (Runner::is_stopping() or (no_update and not current_cpu.cpu_percent[...].empty()))
    return current_cpu;
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Multiple atomic<bool> flags | Single variant + shadow tag | Phase 14 | Runner state is explicit, testable, no invalid combinations |
| Direct flag mutation (main -> runner) | Typed command via conf struct + semaphore | Phase 14 | Communication protocol is documented in types |
| Implicit state (active && !stopping = collecting) | Named state: `RunnerStateTag::Collecting` | Phase 14 | Debugging/logging shows actual state name |

**Dead code removed:**
- `Runner::waiting` (atomic<bool>, defined btop.cpp:416, never read)
- `Runner::reading` (extern declaration btop_shared.hpp:412, never defined)

## Open Questions

1. **Should `Runner::pause_output` become `atomic<bool>`?**
   - What we know: Currently a non-atomic `bool` written by Menu (main thread) and read by runner thread. This is a data race per the C++ memory model, though benign in practice (torn read of a bool is harmless).
   - What's unclear: Whether making it atomic adds measurable overhead. Whether it should move into `runner_conf` entirely (but Menu sets it while runner is active).
   - Recommendation: Make it `atomic<bool>`. The cost is negligible (single byte, already in cache). This eliminates a TSan finding for Phase 15 verification. Do NOT make it a runner FSM state.

2. **Should `Runner::coreNum_reset` be converted to a runner event?**
   - What we know: Set by Linux collector (from within the runner thread itself) when CPU core count changes. Read by the runner loop to trigger a resize. This is an intra-thread flag, not cross-thread.
   - What's unclear: Whether converting it to a runner event adds value vs complexity.
   - Recommendation: Keep as `atomic<bool>`. It is a Linux-specific collector signal internal to the runner thread. Converting it to a runner event would mean the collector pushes an event to itself, which is over-engineering.

3. **Should the runner variant carry `runner_conf` data or use the existing `current_conf` static?**
   - What we know: `runner_conf` is set by the main thread in `Runner::run()` and read by the runner thread after semaphore wake. Currently it is a static within the Runner namespace.
   - What's unclear: Whether moving it into `runner::Collecting` state struct provides meaningful type safety.
   - Recommendation: Keep `current_conf` as a namespace-scoped static for Phase 14. The main thread writes it before semaphore release; the runner thread reads it after acquire. The happens-before relationship is guaranteed by the semaphore. Moving it into the variant would require the main thread to construct the variant (violating runner-thread-owns-variant). This can be revisited if desired but is not required by RUNNER-03.

4. **How should `atomic_wait` and `atomic_wait_for` work with `RunnerStateTag`?**
   - What we know: The existing helpers are typed for `atomic<bool>`. The main thread extensively uses `atomic_wait(Runner::active)` and `atomic_wait_for(Runner::active, true, N)`.
   - What's unclear: Whether to add overloads for `atomic<RunnerStateTag>` or use a wrapper function.
   - Recommendation: Add `Runner::wait_idle()` and `Runner::wait_idle_for(uint64_t ms)` helper functions that wrap `atomic_wait` / `atomic_wait_for` on the runner tag. This avoids generic template complexity and makes call sites self-documenting.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 (via FetchContent) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests -R "Runner" --output-on-failure` |
| Full suite command | `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| RUNNER-01 | RunnerStateVar has exactly 4 alternatives | unit | `ctest --test-dir build/tests -R RunnerState --output-on-failure` | Wave 0 |
| RUNNER-01 | runner::Idle, Collecting, Drawing, Stopping constructible | unit | `ctest --test-dir build/tests -R RunnerState --output-on-failure` | Wave 0 |
| RUNNER-01 | RunnerStateTag enum values correct (0-3) | unit | `ctest --test-dir build/tests -R RunnerState --output-on-failure` | Wave 0 |
| RUNNER-01 | to_string(RunnerStateTag) returns correct names | unit | `ctest --test-dir build/tests -R RunnerState --output-on-failure` | Wave 0 |
| RUNNER-02 | Runner::is_stopping() returns true when tag==Stopping | unit | `ctest --test-dir build/tests -R RunnerCompat --output-on-failure` | Wave 0 |
| RUNNER-02 | Runner::is_active() returns true for Collecting and Drawing | unit | `ctest --test-dir build/tests -R RunnerCompat --output-on-failure` | Wave 0 |
| RUNNER-02 | Runner::is_active() returns false for Idle and Stopping | unit | `ctest --test-dir build/tests -R RunnerCompat --output-on-failure` | Wave 0 |
| RUNNER-02 | Runner::waiting and Runner::reading removed (compile-time) | compile-time | Build succeeds without these symbols | Build |
| RUNNER-03 | Runner::run() sets conf and triggers semaphore (integration) | manual | Build + btop displays data correctly | manual |
| RUNNER-03 | Runner::stop() transitions to Stopping state | unit | `ctest --test-dir build/tests -R RunnerStop --output-on-failure` | Wave 0 |
| RUNNER-04 | Binary semaphore preserved in runner loop | integration (manual) | btop starts, displays data within 1 update_ms | manual |
| RUNNER-04 | No performance regression in runner wake latency | integration (manual) | Measure first-frame latency before/after | manual |
| ALL | All collectors work with Runner::is_stopping() | integration (manual) | btop runs, resize/quit work on all platforms | manual |
| ALL | Full application behavior unchanged | integration (manual) | Resize, Ctrl+C, Ctrl+Z/fg, kill -USR2, menu, key input all work | manual |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . --target btop_test && ctest --test-dir tests --output-on-failure`
- **Per wave merge:** Full suite + manual build and test on macOS
- **Phase gate:** Full suite green + manual verification: resize, Ctrl+C, Ctrl+Z/fg, kill -USR2, key input, menu navigation

### Wave 0 Gaps
- [ ] `tests/test_runner_state.cpp` -- NEW: RunnerStateTag enum tests, variant alternative tests, compatibility wrapper tests
- [ ] `tests/CMakeLists.txt` -- UPDATE: add test_runner_state.cpp to btop_test sources

## Sources

### Primary (HIGH confidence)
- Source code analysis of `src/btop.cpp` lines 410-920 (Runner namespace: all flags, _runner thread loop, run(), stop())
- Source code analysis of `src/btop_shared.hpp` lines 410-421 (Runner extern declarations)
- Source code analysis of `src/btop_state.hpp` (AppStateTag enum, state:: structs, AppStateVar variant, to_tag())
- Source code analysis of `src/btop_events.hpp` (AppEvent variant, EventQueue SPSC pattern)
- Source code analysis of `src/btop.cpp` lines 943-1025 (on_exit/on_enter/transition_to -- Phase 13 patterns)
- Source code analysis of `src/btop_draw.cpp` lines 1057,1529,1718,1982,2190 (Runner::stopping checks)
- Source code analysis of `src/btop_draw.cpp` lines 2748,2762-2763 (Runner::active wait, pause_output, redraw)
- Source code analysis of `src/btop_input.cpp` (8 atomic_wait(Runner::active) calls)
- Source code analysis of `src/btop_menu.cpp` (2 atomic_wait(Runner::active), 4 Runner::pause_output)
- Source code analysis of `src/btop_config.cpp` line 725 (atomic_wait(Runner::active))
- Source code analysis of `src/osx/btop_collect.cpp` (3 Runner::stopping checks)
- Source code analysis of `src/linux/btop_collect.cpp` (6 Runner::stopping, 2 coreNum_reset)
- Source code analysis of `src/freebsd/btop_collect.cpp` (2 Runner::stopping checks)
- Source code analysis of `src/openbsd/btop_collect.cpp` (2 Runner::stopping checks)
- Source code analysis of `src/netbsd/btop_collect.cpp` (2 Runner::stopping checks)
- Source code analysis of `src/btop_tools.hpp` lines 363-378 (atomic_wait, atomic_wait_for, atomic_lock)
- Source code analysis of `src/btop_tools.cpp` lines 538-556 (atomic_wait/atomic_lock implementation)
- `.planning/STATE.md` (orthogonal FSM decision, Phase 10-13 decisions)
- `.planning/REQUIREMENTS.md` (RUNNER-01 through RUNNER-04 definitions)
- `.planning/ROADMAP.md` (Phase 14 success criteria)
- `.planning/phases/13-type-safe-states/13-RESEARCH.md` (Phase 13 patterns: dual representation, shadow tag, entry/exit actions)
- `tests/test_app_state.cpp`, `tests/test_transitions.cpp` (existing test patterns for FSM testing)

### Secondary (MEDIUM confidence)
- [cppreference: std::binary_semaphore](https://en.cppreference.com/w/cpp/thread/counting_semaphore) - Semaphore semantics, happens-before guarantees between release and acquire
- [cppreference: std::variant](https://en.cppreference.com/w/cpp/utility/variant) - Variant single-alternative guarantee

### Tertiary (LOW confidence)
- None. All findings are from direct source code analysis.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Using only std::variant and std::atomic already present in the codebase; no new libraries. Same pattern as Phase 13.
- Architecture: HIGH - Runner thread's flag behavior fully enumerated from source code; all 24 `Runner::stopping` sites, 19 `Runner::active` sites, and all other flag usage mapped. The dual-representation pattern is proven by Phase 13.
- Pitfalls: HIGH - All pitfalls identified from actual cross-thread access analysis (5 collector files, draw code, input/menu/config waits) and from the existing Runner::stop() deadlock-avoidance dance.

**Research date:** 2026-03-01
**Valid until:** 2026-03-31 (stable -- no external dependencies, pure internal refactoring using C++ standard library)
