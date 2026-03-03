# Architecture Research

**Domain:** C++ terminal system monitor — unified DirtyFlags mechanism for btop++ v1.6
**Researched:** 2026-03-03
**Confidence:** HIGH (direct source code analysis of the repository; all integration points verified by tracing code paths)

---

## Context: Existing Architecture (v1.5 Baseline)

v1.6 adds a DirtyFlags bitset to consolidate scattered redraw state. Understanding where dirty-flag
state currently lives is the prerequisite for deciding where to put the unified type and how to wire
it in. This document focuses on integration, not the DirtyFlags type itself.

---

## Current Redraw State Inventory

Seven distinct pieces of redraw/dirty state exist across the codebase. Each has different ownership,
threading semantics, and scope.

### Per-Box `bool redraw` (5 instances + 1 vector)

| Variable | Location | Owner | Semantics |
|----------|----------|-------|-----------|
| `Cpu::redraw` | `btop_draw.cpp`, `Cpu` namespace (file-scope) | Runner thread (draw phase) | Set true to force full box redraw; cleared at end of `Cpu::draw()` |
| `Mem::redraw` | `btop_draw.cpp`, `Mem` namespace (file-scope) | Runner thread (draw phase) | Same pattern |
| `Net::redraw` | `btop_draw.cpp`, `Net` namespace (file-scope) | Runner thread (draw phase) | Same pattern; also set to `true` when interface changes |
| `Proc::redraw` | `btop_draw.cpp`, `Proc` namespace (file-scope) | Runner thread (draw phase) | Same pattern; also set when selection reaches list boundary |
| `Gpu::redraw` | `btop_shared.hpp`, `Gpu` namespace | Runner thread (draw phase) | `vector<bool>` — one entry per GPU panel |
| `Menu::redraw` | `btop_menu.hpp` | Main thread | Signals that menu overlay must be regenerated |

**Pattern:** Each `Xxx::draw(data, force_redraw, data_same)` entry point accepts `force_redraw` from
`runner_conf`. If `force_redraw` is true, the function sets its namespace-local `redraw = true`.
The redraw flag gates a "heavy redraw" branch (box outlines, headers, graph/meter initialization)
and is cleared to `false` at the end of draw. Data-dependent elements (graph updates, process rows)
always render; the redraw flag controls only the static structural elements.

### `runner_conf::force_redraw`

Located in `btop.cpp`, `Runner` namespace:
```cpp
struct runner_conf {
    vector<string> boxes;
    bool no_update;
    bool force_redraw;       // <-- this field
    bool background_update;
    string overlay;
    string clock;
};
static runner_conf current_conf;
```
This is a per-cycle parameter passed to the runner thread by writing to `current_conf` before
`thread_trigger()`. It is the primary way the main thread tells the runner "redraw everything this
cycle". Used at three points in the runner body:
- `Cpu::draw(..., conf.force_redraw, ...)` — forwarded to per-box draw entry
- `Gpu::draw(..., conf.force_redraw, ...)` — same
- `Mem::draw(..., conf.force_redraw, ...)` — same
- `Net::draw(..., conf.force_redraw, ...)` — same
- `Proc::draw(..., conf.force_redraw, ...)` — same
- `screen_buffer.set_force_full()` when `conf.force_redraw` is true (forces full terminal emit)

### `Runner::pending_redraw` (Cross-Thread)

```cpp
// btop.cpp, Runner namespace, file-scope:
static atomic<bool> pending_redraw{false};

void request_redraw() noexcept {
    pending_redraw.store(true, std::memory_order_relaxed);
}
```

This is the only mechanism for cross-thread redraw requests. Called from:
- `Draw::calcSizes()` — after layout recomputation, to force next-cycle full redraw
- `Proc::draw()` (line 2925) — when process list changes requiring structural redraw

The flag is folded into `conf.force_redraw` in `Runner::run()` just before `thread_trigger()`:
```cpp
if (pending_redraw.exchange(false, std::memory_order_relaxed)) {
    current_conf.force_redraw = true;
}
```

### `ScreenBuffer::force_full`

```cpp
// btop_draw.hpp, Draw::ScreenBuffer class:
bool force_full = true;  // private field, starts true (first frame = full emit)

void set_force_full() { force_full = true; }
bool needs_full() const { return force_full; }
void clear_force_full() { force_full = false; }
```

This is owned entirely by the runner thread (the `ScreenBuffer screen_buffer` object is in the
`Runner` namespace in `btop.cpp`). It controls whether `diff_and_emit` or `full_emit` is used.
Set by `screen_buffer.set_force_full()` when `conf.force_redraw` is true. Independent from the
per-box redraw flags; a full screen emit is needed on resize regardless of which boxes were redrawn.

### `Menu::redraw`

```cpp
// btop_menu.hpp:
extern bool redraw;
```

Signals that the menu overlay string should be regenerated. Set in `calcSizes()` when a menu is
active. Set in `on_input()` when menu state changes. Read by `Menu::process()`. This is
main-thread only (menu rendering runs on main thread via `Runner::pause_output`).

### Dead Code: `Proc::resized`

```cpp
// btop_shared.hpp, Proc namespace:
extern atomic<bool> resized;
```

Declared but **never written anywhere in the codebase**. Only read in one place:
```cpp
// btop_draw.cpp:2926:
if (not (Proc::resized or Global::app_state.load() == AppStateTag::Resizing)) {
    Proc::p_counters.clear();
    Proc::p_graphs.clear();
}
```
Since `resized` is always false, the condition reduces to `!Resizing`. This `atomic<bool>` does
nothing and should be removed outright.

### Local `bool redraw` in `btop_input.cpp`

```cpp
// btop_input.cpp, Input::process(), multiple blocks:
bool no_update = true;
bool redraw = true;   // <-- local variable, naming collision with namespace-scoped bool redraws
```

This local exists in four separate `if (Xxx::shown)` blocks. It is passed to `Runner::run(box,
no_update, redraw)` as the `force_redraw` argument. The name shadows nothing (C++ has no shadowing
here since the namespace-scoped `Cpu::redraw`, `Mem::redraw` etc. are in different namespaces), but
it creates conceptual confusion. Renaming it `force_redraw` is the straightforward fix.

---

## System Overview: Current Data Flow

```
MAIN THREAD                              RUNNER THREAD
-----------                              -------------

[Signal handlers]                        _runner() loop:
  push to event_queue                     thread_wait()  (binary_semaphore)
        |                                      |
[Main loop]                              [Collecting state]
  try_pop → dispatch_event                  Cpu::collect()
  → transition_to (App FSM)                 Mem::collect() ...
        |                                      |
[Input polling]                          [Drawing state]
  Input::poll() → Input::get()              Cpu::draw(conf.force_redraw)
  → Input::process(key)                     Mem::draw(conf.force_redraw)
        |                                   ...
  per-box handling sets local              per-box `redraw` bool gates
  `redraw` bool                            structural redraw
  → Runner::run(box, no_update, redraw)         |
        |                                  render_to_buffer()
[Runner::run()]                            diff_and_emit() or full_emit()
  wait for Idle                            (based on force_full latch)
  build current_conf                       write_stdout()
  fold pending_redraw
  thread_trigger()

[Draw::calcSizes()]
  Runner::wait_idle()
  ... geometry computation ...
  Runner::request_redraw()   ←── cross-thread via atomic<bool>
  set per-box redraw = true (line 2938)
```

---

## Target Architecture: v1.6 DirtyFlags Integration

### What Changes

The goal is to replace the scattered state with a single `DirtyFlags` bitset. The key design
decisions that determine integration shape are:

1. **Where does the bitset live?** In the `Runner` namespace in `btop.cpp`, replacing `runner_conf::force_redraw` and `pending_redraw`. The per-box `bool redraw` namespace-level variables in `btop_draw.cpp` are replaced by bits in the same set.

2. **Who reads it?** The runner thread reads it at the start of each draw cycle. The per-box `draw()` functions receive their relevant bit as the existing `bool force_redraw` parameter (interface unchanged) or read it directly from the bitset.

3. **Who writes it?** Main thread writes via `DirtyFlags::mark(bit)`. Cross-thread path (`request_redraw`) uses an `atomic<uint32_t>` or `atomic<DirtyFlags>` so the store is safe.

4. **How does `calcSizes()` decouple?** Currently `calcSizes()` both computes geometry AND marks
   redraws. After v1.6, `calcSizes()` calls `DirtyFlags::mark_all_boxes()` plus
   `DirtyFlags::mark(ForceFullEmit)` instead of directly assigning `Cpu::redraw = true` etc. The
   geometry computation itself is unchanged; only the dirty-marking calls change.

### System Overview: Target Data Flow

```
MAIN THREAD                              RUNNER THREAD
-----------                              -------------

[Input::process()]                       _runner() loop:
  per-box handling                         thread_wait()
  → DirtyFlags::mark(Box::Cpu)                  |
  → Runner::run(box, no_update)            [Collecting state]
        |                                     collect() calls (unchanged)
[Runner::run()]                                   |
  wait for Idle                            [Drawing state]
  read+clear pending dirty bits            local dirty = flags_.exchange(0)
  merge into conf or pass directly         Cpu::draw(dirty & Cpu, ...)
  thread_trigger()                         ...
        |                                  if (dirty & ForceFullEmit)
[Draw::calcSizes()]                            screen_buffer.set_force_full()
  Runner::wait_idle()                      else
  ... geometry ...                             diff_and_emit()
  DirtyFlags::mark_all()                   write_stdout()
  DirtyFlags::mark(ForceFullEmit)

[request_redraw() cross-thread path]
  dirty_pending_.fetch_or(AllBoxes | ForceFullEmit,
                          memory_order_relaxed)
```

### DirtyFlags Type Design

The type belongs in a new header `btop_dirty.hpp` (or can be added to `btop_state.hpp`). The
recommended approach is a standalone header since it is a new type with its own test coverage.

```cpp
// btop_dirty.hpp

enum class DirtyBit : uint32_t {
    Cpu          = 1u << 0,
    Mem          = 1u << 1,
    Net          = 1u << 2,
    Proc         = 1u << 3,
    Gpu0         = 1u << 4,
    Gpu1         = 1u << 5,
    Gpu2         = 1u << 6,
    Gpu3         = 1u << 7,
    Gpu4         = 1u << 8,
    ForceFullEmit = 1u << 9,  // drives screen_buffer.set_force_full()
    MenuRedraw   = 1u << 10,  // drives Menu::redraw
    AllBoxes     = Cpu|Mem|Net|Proc|Gpu0|Gpu1|Gpu2|Gpu3|Gpu4,
    All          = AllBoxes | ForceFullEmit,
};

// Bitwise helpers (constexpr free functions or enable_bitmask_operators)
constexpr DirtyBit operator|(DirtyBit a, DirtyBit b) noexcept;
constexpr DirtyBit operator&(DirtyBit a, DirtyBit b) noexcept;
constexpr bool any(DirtyBit bits) noexcept { return bits != DirtyBit{0}; }

// Thread-safe cross-thread dirty accumulator (written by any thread, read+cleared by runner)
// Uses atomic fetch_or for lock-free accumulation.
struct PendingDirty {
    std::atomic<uint32_t> bits_{0};

    void mark(DirtyBit b) noexcept {
        bits_.fetch_or(static_cast<uint32_t>(b), std::memory_order_relaxed);
    }

    // Atomically read and clear; returns what was pending.
    DirtyBit take() noexcept {
        return static_cast<DirtyBit>(
            bits_.exchange(0, std::memory_order_acq_rel));
    }
};
```

`DirtyBit` is a plain bitmask enum — zero overhead, stack-allocated, trivially copyable. The
`PendingDirty` struct is the cross-thread channel replacing `atomic<bool> pending_redraw`. The
runner thread calls `pending.take()` once per cycle and gets the union of all dirty requests since
last cycle, atomically clearing the accumulator.

For GPU, the number of panels is runtime-determined, but the maximum is 5 (indices 0-4 in
`Gpu::shown_panels`). Five explicit bits is simpler and faster than a `vector<bool>` or dynamic
bitset.

### Component Responsibilities After v1.6

| Component | Responsibility | Communicates With | Threading |
|-----------|---------------|-------------------|-----------|
| `DirtyBit` enum | Per-box and force-emit flags | All dirty-marking sites | N/A — value type |
| `PendingDirty` (in `Runner` namespace) | Atomic accumulator for cross-thread requests | `request_redraw()`, `calcSizes()`, runner loop | Written any thread, read+cleared runner only |
| Per-box `bool redraw` (removed) | Previously gated structural redraw per box | Replaced by DirtyBit | Eliminated |
| `runner_conf::force_redraw` (removed) | Passed force flag to each draw call | Replaced by DirtyBit per cycle | Eliminated |
| `ScreenBuffer::force_full` | Controls full vs diff terminal emit | Set by runner from `ForceFullEmit` bit | Runner thread only |
| `Menu::redraw` | Menu overlay regeneration flag | `calcSizes()`, menu transitions | Main thread only; may stay separate |

---

## Integration Points

### Point 1: `Runner::run()` — Main-Thread Writer

**File:** `btop.cpp`, `Runner::run(box, no_update, force_redraw)`

**Current:**
```cpp
current_conf = {boxes, no_update, force_redraw, ...};
if (pending_redraw.exchange(false)) current_conf.force_redraw = true;
thread_trigger();
```

**After:**
```cpp
// Caller passes force_redraw=true → mark appropriate bits
if (force_redraw) pending_dirty.mark(box_to_dirty_bits(box) | DirtyBit::ForceFullEmit);
// no_update path is unchanged (just skip collect)
current_conf = {boxes, no_update, ...};  // force_redraw field removed
thread_trigger();
```

The helper `box_to_dirty_bits(string_view box)` maps the `box` string to the corresponding
`DirtyBit` set (e.g., `"cpu"` → `DirtyBit::Cpu`, `"all"` → `DirtyBit::AllBoxes`).

**Thread safety:** `pending_dirty.mark()` uses `fetch_or(relaxed)` — safe to call from main thread
while runner is idle. Runner always checks dirty bits after acquiring the lock (it calls
`thread_wait()` first, so main thread and runner do not race on the bitset when runner is active).

### Point 2: `Runner::_runner()` — Runner-Thread Reader

**File:** `btop.cpp`, static `_runner()` body

**Current:**
```cpp
auto& conf = current_conf;
// ... later:
if (not pause_output) output += Cpu::draw(cpu, conf.force_redraw, conf.no_update);
// ... and before emitting:
if (conf.force_redraw) { screen_buffer.set_force_full(); }
```

**After:**
```cpp
// Once per cycle, at start of Drawing phase:
const DirtyBit dirty = pending_dirty.take();
const bool force_cpu  = any(dirty & DirtyBit::Cpu);
const bool force_mem  = any(dirty & DirtyBit::Mem);
// etc.

// Draw calls become:
if (not pause_output) output += Cpu::draw(cpu, force_cpu, conf.no_update);
// ...
if (any(dirty & DirtyBit::ForceFullEmit)) screen_buffer.set_force_full();
```

`pending_dirty.take()` is the only read+clear operation and must happen on the runner thread. The
decomposition into per-box booleans before the draw calls allows the existing `Xxx::draw()` function
signatures to remain unchanged, making this a conservative integration.

**Alternative (more invasive):** Pass `dirty` directly to draw functions; remove the per-box `bool
force_redraw` parameter in favor of the enum. This is cleaner long-term but changes all five draw
function signatures and all call sites. The conservative approach is preferred for the first phase.

### Point 3: `Draw::calcSizes()` — Decoupled Layout/Dirty Marking

**File:** `btop_draw.cpp`, `Draw::calcSizes()`

**Current (lines 2924-2938):**
```cpp
Runner::request_redraw();                        // marks pending_redraw atomic
if (Input::is_menu_active()) Menu::redraw = true;
// ...
Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true;  // direct writes
```

**After:**
```cpp
// Geometry computation (unchanged): computes x/y/width/height for all boxes
// After geometry is done:
pending_dirty.mark(DirtyBit::All);              // replaces request_redraw() + direct writes
if (Input::is_menu_active()) Menu::redraw = true;  // unchanged (Menu::redraw stays)
// The direct `Cpu::redraw = ... = true` assignments disappear entirely
```

This is the primary **decoupling**: previously `calcSizes()` had side effects on namespace-scoped
booleans across four different namespaces. After this change, it calls one function on one object.
The geometry computation is entirely unchanged; only the post-computation dirty-marking changes.

The `Gpu::redraw.resize(shown)` and `redraw[i] = true` at lines 3054-3057 are analogous: they
become `for (int i = 0; i < shown; ++i) pending_dirty.mark(gpu_dirty_bit(i))`.

### Point 4: `Runner::request_redraw()` — Cross-Thread API

**File:** `btop_shared.hpp` (declaration), `btop.cpp` (definition)

**Current:**
```cpp
static atomic<bool> pending_redraw{false};
void request_redraw() noexcept {
    pending_redraw.store(true, std::memory_order_relaxed);
}
```

**After:**
```cpp
void request_redraw() noexcept {
    pending_dirty.mark(DirtyBit::All);  // same semantics: force all boxes + full emit
}
```

The public `request_redraw()` interface is retained — callers in `btop_draw.cpp:2925` do not need
to change. Only the implementation changes. This satisfies the Proc::draw() call site and any other
future callers without cascading changes.

### Point 5: `btop_input.cpp` — Naming Collision Fix

**File:** `btop_input.cpp`, `Input::process()`

Four blocks contain:
```cpp
bool no_update = true;
bool redraw = true;         // confusing: same name as Cpu::redraw, Mem::redraw etc.
// ...
Runner::run("proc", no_update, redraw);
```

**Fix:** Rename local variable `redraw` to `force_redraw` in all four blocks. The semantics do not
change; this is purely a naming fix to eliminate the collision with the namespace-scoped `bool
redraw` variables being removed. After v1.6, the namespace-scoped booleans are gone, so the
collision is resolved regardless, but the rename makes the intent explicit while both exist during
the transition.

### Point 6: `Proc::resized` — Dead Code Removal

**File:** `btop_shared.hpp` (declaration), platform-specific `btop_linux.cpp` etc. for definition

Remove the `extern atomic<bool> resized;` declaration and its definition. Update the one read site
in `btop_draw.cpp:2926`:
```cpp
// Before:
if (not (Proc::resized or Global::app_state.load() == AppStateTag::Resizing)) {
// After:
if (Global::app_state.load() != AppStateTag::Resizing) {
```
This is behavior-preserving: `Proc::resized` was always false, so the condition was already
equivalent to the simplified form.

---

## Data Flow: Full Redraw Request (e.g., after resize)

```
SIGWINCH signal handler
    → event_queue.push(event::Resize{})

Main loop
    → dispatch_event(Running, Resize{}) → transition_to(Resizing)
    → on_enter(Resizing, ctx):
         term_resize(true)
         Draw::calcSizes()         ← calls pending_dirty.mark(DirtyBit::All)
         Runner::screen_buffer.resize(w, h)   ← sets force_full = true separately
         Runner::run("all", true, true)        ← sets force_redraw arg = true
              → Runner::run: pending_dirty.mark(AllBoxes | ForceFullEmit)
              → thread_trigger()

Runner thread wakes:
    dirty = pending_dirty.take()  → All bits set
    Cpu::draw(cpu, true, ...)     → Cpu namespace: force_redraw=true → redraw=true
    ...
    if (any(dirty & ForceFullEmit)) screen_buffer.set_force_full()
    full_emit() → write_stdout()
```

## Data Flow: Single-Box Redraw (e.g., user toggles proc sort)

```
Input::process("left")
    → Proc::shown block
    → local force_redraw = true  (was: local redraw = true)
    → Runner::run("proc", no_update=true, force_redraw=true)

Runner::run("proc", true, true):
    pending_dirty.mark(DirtyBit::Proc | DirtyBit::ForceFullEmit)
    current_conf = {["proc"], no_update=true}
    thread_trigger()

Runner thread:
    dirty = pending_dirty.take()  → Proc | ForceFullEmit bits
    conf.boxes = ["proc"], so only Proc::draw() is called
    Proc::draw(proc, force_proc=true, ...)
    if (any(dirty & ForceFullEmit)) screen_buffer.set_force_full()
    full_emit()   ← force_redraw currently also triggers full_emit, consistent
```

Note: Whether single-box redraws should use `diff_and_emit` instead of `full_emit` is a separate
optimization question. The current behavior (force_redraw always triggers full_emit) is preserved.

## Data Flow: Cross-Thread Redraw Request (from draw function)

```
Proc::draw() detects condition requiring structural redraw
    → Runner::request_redraw()
        → pending_dirty.mark(DirtyBit::All)   (atomic fetch_or, relaxed order)

Next Runner::run() call (next timer tick):
    pending_dirty.take() returns All bits
    → force_redraw on all boxes next cycle
```

---

## Build Order and Phase Dependencies

Five phases are indicated by the four v1.6 requirements plus their logical dependencies:

```
Phase A: DirtyFlags type (btop_dirty.hpp)
  NEW FILE — no dependencies on existing code
  Provides: DirtyBit enum, PendingDirty struct, helper functions
  Tests: unit tests for bitmask operations, take() atomicity
         |
         v
Phase B: Remove dead Proc::resized + rename local redraw vars
  MODIFY btop_shared.hpp, platform btop_*.cpp, btop_draw.cpp, btop_input.cpp
  No new dependencies; independent cleanup
  Tests: existing 330 tests pass, compile clean on all platforms
         |
         v
Phase C: Wire PendingDirty into Runner (replaces pending_redraw + runner_conf::force_redraw)
  MODIFY btop.cpp: Runner::run(), _runner() body
  Depends on Phase A (needs PendingDirty)
  Removes: pending_redraw atomic<bool>, force_redraw field from runner_conf
  Tests: functional test that force_redraw requests propagate correctly
         |
         v
Phase D: Decouple calcSizes() (remove per-box redraw direct writes)
  MODIFY btop_draw.cpp: calcSizes() body
  Depends on Phase C (PendingDirty must exist before calcSizes() can call it)
  Removes: Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true in calcSizes()
  Removes: Runner::request_redraw() call in calcSizes() (replaced by pending_dirty.mark)
  Tests: resize triggers full redraw, box toggle triggers full redraw
         |
         v
Phase E: Remove per-box namespace redraw booleans
  MODIFY btop_draw.cpp: remove Cpu::redraw, Mem::redraw, Net::redraw, Proc::redraw, Gpu::redraw
  Depends on Phase C+D (the booleans are now driven by per-cycle dirty bits from runner)
  Also removes the draw function bodies that write to these booleans
  Tests: full regression suite (330 tests) + ASan/UBSan/TSan clean
```

**Why this order:**

- **A before C/D:** `PendingDirty` must exist before any integration site uses it.
- **B independent:** Dead code removal and rename do not depend on the new type. Can ship B first to reduce diff noise, or merge with A.
- **C before D:** `calcSizes()` must have somewhere to send dirty bits before its direct namespace writes are removed.
- **D before E:** Per-box `Xxx::redraw` booleans remain as the short-circuit inside each draw function until Phase E. They are set by the `if (force_redraw) redraw = true` idiom. Phase E removes the namespace-level bool storage; the draw functions then derive the redraw decision directly from the `force_redraw` parameter without storing it.
- **E last:** Removing the booleans is the final step because it has the most coupling (the draw functions both read and write them).

**Alternative ordering:** B can be merged with A or E since it touches different sites. Splitting B
out is recommended to keep each phase's diff focused and reviewable.

---

## Architectural Patterns

### Pattern 1: Atomic Fetch-Or Accumulator

**What:** A `uint32_t atomic` accumulates dirty bits from any producer thread via `fetch_or`. The
single consumer (runner thread) calls `exchange(0)` to atomically read-and-clear.
**When to use:** When multiple sources can independently mark dirty state, but there is exactly one
consumer that processes the accumulated state and clears it.
**Trade-offs:** `fetch_or` is a single instruction on x86/ARM — zero contention overhead. The
`exchange(0)` in the runner guarantees no bits are lost between the `fetch_or` on one thread and
the `exchange` on another, because `acq_rel` ordering on `exchange` ensures any stores visible
before the exchange are observed.

```cpp
// Any thread (signal-safe with relaxed ordering):
pending_dirty.bits_.fetch_or(
    static_cast<uint32_t>(DirtyBit::Cpu),
    std::memory_order_relaxed);

// Runner thread once per cycle:
uint32_t bits = pending_dirty.bits_.exchange(0, std::memory_order_acq_rel);
DirtyBit dirty = static_cast<DirtyBit>(bits);
```

### Pattern 2: Conservative Parameter-Preservation

**What:** The per-box `draw()` function signatures (`bool force_redraw`) are preserved. The
`DirtyBit` is consumed in the runner loop body, which derives the per-box boolean before calling
each draw function. This decouples the new DirtyFlags type from the draw function API.
**When to use:** When the surface area of a refactor must be minimized for risk control, and the
draw functions are already well-tested.
**Trade-offs:** Leaves a thin translation layer in the runner loop. A follow-on phase could remove
the `bool force_redraw` parameters entirely and have draw functions accept `DirtyBit`.

### Pattern 3: Decouple Computation from Side Effects

**What:** `calcSizes()` is responsible for geometry computation. It currently has a side effect of
directly writing to `Cpu::redraw`, `Mem::redraw`, etc. in separate namespaces. Replacing those
writes with a single `pending_dirty.mark(DirtyBit::All)` call separates computation from state
mutation: `calcSizes()` says "all dirty" without knowing which individual flags exist.
**When to use:** When a function with a clear primary responsibility has accreted secondary state
mutation responsibilities. The geometry function should not know about rendering state names.

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Atomic DirtyFlags with `std::atomic<DirtyBit>`

**What people do:** Use `std::atomic<DirtyBit>` directly, leading to a required
`compare_exchange_weak` loop for accumulation (since there is no `fetch_or` for enum types).
**Why it's wrong:** The loop-on-CAS approach is more complex and potentially slower than
`fetch_or` on a `uint32_t`. The enum is just a strongly-typed wrapper around a bitmask integer;
the atomic should operate on the underlying integer type.
**Do this instead:** `std::atomic<uint32_t>` with explicit cast on read. The `PendingDirty` struct
encapsulates this pattern cleanly.

### Anti-Pattern 2: Keeping `runner_conf::force_redraw` Alongside DirtyFlags

**What people do:** Add `DirtyBit` to `runner_conf` in addition to `bool force_redraw`, treating
them as parallel channels.
**Why it's wrong:** Two mechanisms for the same concept causes double-application: the runner would
have to check both. Forces are cumulative OR — a single bitset covers both cases.
**Do this instead:** Remove `runner_conf::force_redraw` entirely. The runner reads `pending_dirty.take()` once and that is the only redraw decision source.

### Anti-Pattern 3: Per-Box Dirty Objects

**What people do:** Add a `bool dirty` or `DirtyBit` to each box namespace (`Cpu::dirty`,
`Mem::dirty`, etc.) instead of a single shared accumulator.
**Why it's wrong:** Recreates the scattering problem being solved. Forces direct writes across
namespace boundaries, same as today. Cross-thread access requires each one to be atomic, multiplying
the atomic count.
**Do this instead:** One `PendingDirty` object in the `Runner` namespace. All writers go through
it. One reader (runner thread) takes the combined state.

### Anti-Pattern 4: Routing `calcSizes()` Through the Event Queue

**What people do:** Push a `Resize` event after `calcSizes()` to trigger a redraw.
**Why it's wrong:** `calcSizes()` is called from the main thread (and sometimes from within
`on_enter(Resizing)` which is already handling a Resize event). Pushing an event from within an
event handler creates re-entrancy and potential infinite loop. The event queue is for
signal-handler-to-main-loop communication, not for synchronous in-loop state changes.
**Do this instead:** `calcSizes()` calls `pending_dirty.mark()` directly. No event involved.

### Anti-Pattern 5: Removing `Menu::redraw` into DirtyFlags

**What people do:** Add `MenuRedraw` bit to `DirtyBit` and replace `Menu::redraw` with it.
**Why it's wrong:** `Menu::redraw` is main-thread only and consumed synchronously within `Menu::process()`. It is not a cross-thread concern. Adding it to an atomic accumulator adds complexity without benefit. The menu rendering path does not go through the runner thread.
**Do this instead:** Leave `Menu::redraw` as-is. Optionally add a `MenuRedraw` bit as a convenience
alias if future phases need it, but don't migrate it in v1.6.

---

## New vs Modified Components

### New

| Component | File | Purpose |
|-----------|------|---------|
| `DirtyBit` enum | `src/btop_dirty.hpp` (new) | Bitmask type for per-box and force-emit flags |
| `PendingDirty` struct | `src/btop_dirty.hpp` (new) | Atomic accumulator replacing `pending_redraw` |
| `pending_dirty` instance | `src/btop.cpp`, `Runner` namespace | Global instance, runner owns read; any thread can write |

### Modified

| Component | File | Change |
|-----------|------|--------|
| `runner_conf` | `src/btop.cpp` | Remove `bool force_redraw` field |
| `Runner::run()` | `src/btop.cpp` | Replace force_redraw param handling with `pending_dirty.mark()` |
| `Runner::_runner()` | `src/btop.cpp` | Replace `conf.force_redraw` checks with `pending_dirty.take()` |
| `Runner::request_redraw()` | `src/btop.cpp` | Implementation: replaces `pending_redraw.store()` |
| `pending_redraw` | `src/btop.cpp` | Removed (replaced by `pending_dirty`) |
| `Draw::calcSizes()` | `src/btop_draw.cpp` | Replace direct namespace redraw writes with `pending_dirty.mark(All)` |
| `Cpu::redraw` (namespace bool) | `src/btop_draw.cpp` | Removed; per-cycle force derived from `force_redraw` param |
| `Mem::redraw` (namespace bool) | `src/btop_draw.cpp` | Same |
| `Net::redraw` (namespace bool) | `src/btop_draw.cpp` | Same |
| `Proc::redraw` (namespace bool) | `src/btop_draw.cpp` | Same |
| `Gpu::redraw` (vector<bool>) | `src/btop_draw.cpp` | Replaced by `DirtyBit::Gpu0`..`Gpu4` in pending_dirty |
| `Proc::resized` | `src/btop_shared.hpp` | Removed (dead code) |
| Local `bool redraw` in `Input::process` | `src/btop_input.cpp` | Renamed to `force_redraw` |

### Unchanged

| Component | Notes |
|-----------|-------|
| `ScreenBuffer::force_full` | Still set by runner from `ForceFullEmit` bit; internal to runner |
| `Menu::redraw` | Main-thread only; not part of cross-thread DirtyFlags |
| All `Xxx::draw()` signatures | `bool force_redraw` parameter preserved in Phase C; removed in Phase E optional follow-on |
| `AppStateVar`, `RunnerStateTag` | Entirely independent of dirty-flag mechanism |
| `EventQueue`, `AppEvent` | No changes needed |
| `Input FSM`, `Menu PDA` | No changes needed |
| All collector functions | No changes needed |

---

## Thread-Safety Implications

### PendingDirty is the Only Cross-Thread Object

All previous per-box `bool redraw` variables are **runner-thread-only** in the target architecture.
They are read at start of each cycle (`pending_dirty.take()`) and consumed locally. No other thread
touches them.

`Menu::redraw` is **main-thread-only**. No threading concern.

`pending_dirty.bits_` is `atomic<uint32_t>`. Producers call `fetch_or(relaxed)`. The runner calls
`exchange(0, acq_rel)`. The `acq_rel` on exchange ensures the runner sees all marks that happened
before the exchange on any thread. Producers do not need acquire ordering because they do not read
back the value — they only accumulate.

### Race Condition: mark() vs take() during Runner::run()

`Runner::run()` is called from the main thread. It calls `pending_dirty.mark()` and then
`thread_trigger()`. The runner thread is guaranteed to be idle (the `Runner::run()` body waits for
idle before proceeding). Therefore, `pending_dirty.mark()` in `Runner::run()` and
`pending_dirty.take()` in `_runner()` do not execute concurrently for the same cycle. The
`thread_trigger()` acts as the synchronization point: marks happen before trigger; take() happens
after trigger (inside the runner loop, after `thread_wait()`).

### Race Condition: request_redraw() from Proc::draw()

`Proc::draw()` is called from the runner thread. `request_redraw()` inside it writes to
`pending_dirty` from the runner thread. This is the only case where `pending_dirty` is written from
within the runner. Since the runner will call `take()` at the start of the **next** cycle (not the
current one), this write is safe — it is accumulated for the next wake-up. The runner does not call
`take()` mid-cycle.

---

## Sources

- Direct source code analysis: all integration points verified by reading `src/btop.cpp` (Runner namespace, lines 424-955), `src/btop_draw.cpp` (calcSizes, all box draw functions), `src/btop_shared.hpp` (box namespace declarations), `src/btop_input.cpp` (Input::process)
- Existing DirtyFlags patterns: C++ `<bitset>`, `std::atomic` documentation (standard library, no external sources needed)
- Project context: `.planning/PROJECT.md`
- Prior architecture research: `.planning/research/ARCHITECTURE.md` (v1.3 baseline for App FSM + Runner FSM)

---
*Architecture research for: btop++ v1.6 unified DirtyFlags mechanism*
*Researched: 2026-03-03*
