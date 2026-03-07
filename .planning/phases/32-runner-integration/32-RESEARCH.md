# Phase 32: Runner Integration - Research

**Researched:** 2026-03-07
**Domain:** C++ runner thread wiring, atomic state replacement, dirty flag consumption
**Confidence:** HIGH

## Summary

Phase 32 replaces the existing `pending_redraw` atomic bool and `runner_conf::force_redraw` bool with the `PendingDirty` infrastructure created in Phase 31. The runner thread currently uses a single `conf.force_redraw` bool that gets passed to every draw function identically. Phase 32 changes the runner to call `pending_dirty.take()` once per draw cycle, derive per-box booleans from the result, and pass those to each draw function. The `ForceFullEmit` bit drives `screen_buffer.set_force_full()` exclusively, separating full-emit from per-box dirty state.

The scope is tightly bounded: changes are primarily in `src/btop.cpp` (Runner namespace) with minor touch points in `src/btop_shared.hpp` (declaration changes) and `src/btop_draw.cpp` (`request_redraw()` call site in `calcSizes()`). Draw function signatures do NOT change in this phase -- the existing `bool force_redraw` parameter stays; what changes is how that bool is derived (from per-box DirtyBit instead of a single global bool).

This is a refactoring phase with no new features. The key risk is breaking the redraw chain: if any site that previously set `pending_redraw` or `force_redraw` is missed, boxes will fail to redraw on resize or user input. The mitigation is that all call sites are identified below and the compiler will catch type mismatches.

**Primary recommendation:** Replace `pending_redraw` with a `PendingDirty` instance, modify `Runner::run()` to accept dirty bits instead of a force_redraw bool, and have the thread loop decompose the taken bits into per-box booleans passed to each `draw()` call. Keep draw function signatures unchanged.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| WIRE-01 | pending_redraw atomic\<bool\> replaced by PendingDirty instance | Replace `static atomic<bool> pending_redraw` (btop.cpp:428) with `static PendingDirty pending_dirty` |
| WIRE-02 | runner_conf::force_redraw replaced by per-box dirty bits from take() | Remove `bool force_redraw` from runner_conf (btop.cpp:493); add dirty bits field or derive in thread loop |
| WIRE-03 | Draw functions receive per-box dirty state instead of single force_redraw bool | Thread loop passes `has_bit(dirty, DirtyBit::Cpu)` to Cpu::draw(), etc. instead of `conf.force_redraw` |
| WIRE-04 | ScreenBuffer::force_full driven by ForceFullEmit bit, kept separate from per-box bits | `has_bit(dirty, DirtyBit::ForceFullEmit)` drives `screen_buffer.set_force_full()` instead of `conf.force_redraw` |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++23 std::atomic | N/A | Lock-free atomic operations | Already used throughout codebase |
| btop_dirty.hpp | N/A (Phase 31) | DirtyBit enum + PendingDirty struct | Created in Phase 31, ready for use |
| GTest | v1.17.0 | Unit testing | Existing test infrastructure |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ThreadSanitizer (TSan) | Compiler-bundled | Verify concurrent access patterns | TSan build to validate no data races after wiring |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Storing DirtyBit in runner_conf | Taking dirty bits in thread loop directly | Taking in thread loop is cleaner -- avoids storing atomically-derived state in a non-atomic struct |
| Keeping force_redraw in runner_conf | Replacing with DirtyBit field | DirtyBit field is richer; force_redraw was always derived from pending_redraw anyway |

**Installation:**
No new dependencies. Uses btop_dirty.hpp from Phase 31.

## Architecture Patterns

### Current Architecture (Before Phase 32)

```
Main thread (input/signal):
  Runner::run("all", no_update, force_redraw=true)
    -> sets runner_conf { .force_redraw = true }
    -> folds pending_redraw into conf.force_redraw
    -> triggers thread

Runner thread (_runner):
  -> reads conf.force_redraw (single bool)
  -> passes conf.force_redraw to ALL draw functions identically
  -> if conf.force_redraw: screen_buffer.set_force_full()
  -> if screen_buffer.needs_full() || conf.force_redraw: full_emit()
```

### Target Architecture (After Phase 32)

```
Main thread (input/signal):
  Runner::run("all", no_update)           // no force_redraw param
    -> pending_dirty.mark(DirtyAll)       // or specific bits
    -> triggers thread

Runner thread (_runner):
  -> DirtyBit dirty = pending_dirty.take()  // once per cycle
  -> passes has_bit(dirty, DirtyBit::Cpu) to Cpu::draw()
  -> passes has_bit(dirty, DirtyBit::Mem) to Mem::draw()
  -> passes has_bit(dirty, DirtyBit::Net) to Net::draw()
  -> passes has_bit(dirty, DirtyBit::Proc) to Proc::draw()
  -> passes has_bit(dirty, DirtyBit::Gpu) to Gpu::draw()
  -> if has_bit(dirty, DirtyBit::ForceFullEmit): screen_buffer.set_force_full()
  -> if screen_buffer.needs_full(): full_emit()  // NOT dirty-dependent
```

### Pattern 1: Runner::run() Signature Change
**What:** Remove `force_redraw` parameter from `Runner::run()`. Callers that previously passed `force_redraw=true` now call `pending_dirty.mark()` before `run()`.
**When to use:** Every call site that currently passes `force_redraw=true`.

**Current signature (btop_shared.hpp:457):**
```cpp
void run(const string& box = "", bool no_update = false, bool force_redraw = false);
```

**New signature:**
```cpp
void run(const string& box = "", bool no_update = false);
```

**Call site migration pattern:**
```cpp
// BEFORE:
Runner::run("all", false, true);

// AFTER:
Runner::pending_dirty.mark(DirtyAll);
Runner::run("all", false);

// Or for single-box from input:
// BEFORE:
Runner::run("proc", no_update, force_redraw);

// AFTER:
if (force_redraw) Runner::pending_dirty.mark(DirtyBit::Proc);
Runner::run("proc", no_update);
```

### Pattern 2: Thread Loop Dirty Bit Decomposition
**What:** At the top of each draw cycle in `_runner()`, take the accumulated dirty bits and derive per-box booleans.
**When to use:** Once per draw cycle, replacing the current `conf.force_redraw` usage.

```cpp
// In _runner(), after conf is read:
DirtyBit dirty = pending_dirty.take();

// Derive per-box force_redraw booleans
const bool cpu_dirty = has_bit(dirty, DirtyBit::Cpu);
const bool mem_dirty = has_bit(dirty, DirtyBit::Mem);
const bool net_dirty = has_bit(dirty, DirtyBit::Net);
const bool proc_dirty = has_bit(dirty, DirtyBit::Proc);
const bool gpu_dirty = has_bit(dirty, DirtyBit::Gpu);
const bool force_full = has_bit(dirty, DirtyBit::ForceFullEmit);

// Pass to draw functions:
output += Cpu::draw(cpu, gpus_ref, cpu_dirty, conf.no_update);
output += Mem::draw(mem, mem_dirty, conf.no_update);
output += Net::draw(net, net_dirty, conf.no_update);
output += Proc::draw(proc, proc_dirty, conf.no_update);
output += Gpu::draw(gpus_ref[i], i, gpu_dirty, conf.no_update);

// ForceFullEmit drives screen_buffer, NOT draw functions:
if (force_full) {
    empty_bg.clear();
    screen_buffer.set_force_full();
}
```

### Pattern 3: request_redraw() Migration
**What:** `request_redraw()` now marks `DirtyAll` on `pending_dirty` instead of storing to `pending_redraw`.
**When to use:** The single call site in calcSizes() (btop_draw.cpp:2923).

```cpp
// BEFORE:
void request_redraw() noexcept {
    pending_redraw.store(true, std::memory_order_relaxed);
}

// AFTER:
void request_redraw() noexcept {
    pending_dirty.mark(DirtyAll | DirtyBit::ForceFullEmit);
}
```

Note: `request_redraw()` is called from `calcSizes()` which runs after resize/box toggle. It should mark ALL boxes dirty AND ForceFullEmit, since a resize needs full screen repaint.

### Anti-Patterns to Avoid
- **Changing draw function signatures:** Phase 32 keeps `bool force_redraw` parameter on all draw functions. Phase 34 removes it. The parameter name stays as-is; only the source of its value changes.
- **Removing per-box namespace `redraw` bools:** These stay until Phase 34. Phase 32 only changes how the `force_redraw` parameter passed to draw() is derived.
- **Taking dirty bits multiple times per cycle:** Call `take()` exactly once at the start of each cycle. Multiple takes would lose bits between calls.
- **Using pending_dirty.mark() with relaxed ordering:** PendingDirty already uses release/acquire internally. Callers just call `mark()`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Dirty bit accumulation | Manual atomic bool per box | PendingDirty from btop_dirty.hpp | Already built, tested, lock-free |
| Box-to-bit mapping | switch/case or string comparison | Direct DirtyBit enum values | Compile-time, type-safe |

**Key insight:** The mapping from box names to DirtyBit values must be consistent across all call sites. A helper function or lookup could help but the number of boxes (5) is small enough that direct mapping is clearest.

## Common Pitfalls

### Pitfall 1: Missing Call Sites for Runner::run() with force_redraw=true
**What goes wrong:** A call site still passes `force_redraw=true` to the old signature but the parameter is removed. Or worse, if the parameter is just ignored, redraw stops working.
**Why it happens:** There are 13 call sites across 3 files (btop.cpp, btop_input.cpp, btop_menu.cpp).
**How to avoid:** Remove the parameter from the declaration. The compiler will catch all callers. Then migrate each caller to use `pending_dirty.mark()`.
**Complete call site list:**
- btop.cpp:1014 -- `Runner::run("all", true, true)` (initial startup)
- btop.cpp:1550 -- `Runner::run("clock")` (no force_redraw, just remove param)
- btop.cpp:1556 -- `Runner::run("all")` (no force_redraw, just remove param)
- btop_menu.cpp:1653 -- `Runner::run("all", false, true)`
- btop_menu.cpp:1834 -- `Runner::run("all", true, true)`
- btop_menu.cpp:1869 -- `Runner::run("all", true, true)`
- btop_menu.cpp:1872 -- `Runner::run("overlay")` (no force_redraw)
- btop_input.cpp:313 -- `Runner::run("all", false, true)`
- btop_input.cpp:338 -- `Runner::run("all", false, true)`
- btop_input.cpp:578 -- `Runner::run("proc", no_update, force_redraw)`
- btop_input.cpp:609 -- `Runner::run("cpu", no_update, force_redraw)`
- btop_input.cpp:631 -- `Runner::run("mem", no_update, force_redraw)`
- btop_input.cpp:680 -- `Runner::run("net", no_update, force_redraw)`

### Pitfall 2: ForceFullEmit Conflation with Per-Box Dirty
**What goes wrong:** Using ForceFullEmit where only per-box dirty is needed, causing unnecessary full screen repaints on every key press.
**Why it happens:** The old code used `force_redraw` for both "this box needs to redraw its content" AND "the entire screen buffer needs full emit". These are separate concerns.
**How to avoid:**
- Input key presses that trigger single-box redraws: mark only the relevant box bit (e.g., `DirtyBit::Proc`)
- Resize/calcSizes: mark `DirtyAll | DirtyBit::ForceFullEmit`
- Normal timer-driven updates: mark `DirtyAll` (no ForceFullEmit -- differential emit handles it)
**Warning signs:** If every key press causes a full terminal repaint instead of differential, ForceFullEmit is being set too broadly.

### Pitfall 3: Forgetting the pending_redraw Fold Logic
**What goes wrong:** The old code at btop.cpp:902-905 folds `pending_redraw` into `conf.force_redraw` at the point of triggering the thread. If this fold is not replaced, `request_redraw()` calls from other threads (calcSizes) would be lost.
**Why it happens:** The fold logic is subtle -- it happens AFTER conf is set but BEFORE thread_trigger().
**How to avoid:** With PendingDirty, the fold is no longer needed. The thread loop itself calls `take()`, which atomically captures whatever bits have been accumulated. The `run()` function marks bits, and the thread loop takes them. No separate fold step is needed.

### Pitfall 4: Race Between run() Mark and Thread take()
**What goes wrong:** `run()` marks dirty bits, then `thread_trigger()`. If the thread is already running from a previous cycle, the new bits could be taken by the current cycle or the next one.
**Why it happens:** PendingDirty accumulates with `fetch_or`, so bits marked in `run()` will be picked up whenever the thread next calls `take()`.
**How to avoid:** This is actually fine -- `fetch_or` accumulates, so no bits are lost. The only question is which cycle picks them up. This is the same semantics as the current `pending_redraw` boolean.

### Pitfall 5: debug_bg Clearing Depends on force_redraw
**What goes wrong:** At btop.cpp:567, `debug_bg` is recreated when `conf.force_redraw` is true. If this check is changed incorrectly, the debug overlay won't refresh on resize.
**Why it happens:** The debug_bg needs to be redrawn when any box is dirty (since the layout may have changed).
**How to avoid:** Replace `conf.force_redraw` check with a check for any dirty bit being set: `has_bit(dirty, DirtyAll)` or just `static_cast<uint32_t>(dirty) != 0`.

## Code Examples

### Complete runner_conf Change
```cpp
// BEFORE (btop.cpp:490-497):
struct runner_conf {
    vector<string> boxes;
    bool no_update;
    bool force_redraw;    // REMOVE
    bool background_update;
    string overlay;
    string clock;
};

// AFTER:
struct runner_conf {
    vector<string> boxes;
    bool no_update;
    bool background_update;
    string overlay;
    string clock;
};
```

### Complete run() Change
```cpp
// BEFORE (btop.cpp:839):
void run(const string& box, bool no_update, bool force_redraw) {
    // ... wait/restart logic unchanged ...

    current_conf = {
        (box == "all" ? Config::current_boxes : vector{box}),
        no_update, force_redraw,  // force_redraw in struct
        (not Config::getB(BoolKey::tty_mode) and Config::getB(BoolKey::background_update)),
        Global::overlay,
        Global::clock
    };

    //? Fold any pending_redraw into conf.force_redraw
    if (pending_redraw.exchange(false, std::memory_order_relaxed)) {
        current_conf.force_redraw = true;
    }

    thread_trigger();
}

// AFTER:
void run(const string& box, bool no_update) {
    // ... wait/restart logic unchanged ...

    current_conf = {
        (box == "all" ? Config::current_boxes : vector{box}),
        no_update,
        (not Config::getB(BoolKey::tty_mode) and Config::getB(BoolKey::background_update)),
        Global::overlay,
        Global::clock
    };

    // No fold needed -- pending_dirty.take() in thread loop captures all accumulated bits

    thread_trigger();
}
```

### Complete Thread Loop Change (draw section)
```cpp
// At top of draw cycle, after conf is read:
auto& conf = current_conf;

// Take accumulated dirty bits (replaces pending_redraw fold)
DirtyBit dirty = pending_dirty.take();

// Derive per-box state
const bool cpu_dirty = has_bit(dirty, DirtyBit::Cpu);
const bool mem_dirty = has_bit(dirty, DirtyBit::Mem);
const bool net_dirty = has_bit(dirty, DirtyBit::Net);
const bool proc_dirty = has_bit(dirty, DirtyBit::Proc);
const bool gpu_dirty = has_bit(dirty, DirtyBit::Gpu);
const bool force_full = has_bit(dirty, DirtyBit::ForceFullEmit);

// ... collection unchanged ...

// Draw calls use per-box dirty instead of conf.force_redraw:
output += Cpu::draw(cpu, gpus_ref, cpu_dirty, conf.no_update);
output += Gpu::draw(gpus_ref[gpu_panels[i]], i, gpu_dirty, conf.no_update);
output += Mem::draw(mem, mem_dirty, conf.no_update);
output += Net::draw(net, net_dirty, conf.no_update);
output += Proc::draw(proc, proc_dirty, conf.no_update);

// ForceFullEmit drives screen_buffer exclusively:
if (force_full) {
    empty_bg.clear();
    screen_buffer.set_force_full();
}

// Emit logic simplified:
if (screen_buffer.needs_full()) {    // No longer || conf.force_redraw
    Draw::full_emit(screen_buffer, diff_output);
    screen_buffer.clear_force_full();
} else {
    Draw::diff_and_emit(screen_buffer, diff_output);
}
```

### btop_input.cpp Call Site Migration
```cpp
// BEFORE (single-box input, e.g., proc at line 578):
Runner::run("proc", no_update, force_redraw);

// AFTER:
if (force_redraw) Runner::pending_dirty.mark(DirtyBit::Proc);
Runner::run("proc", no_update);

// BEFORE (all-box with force_redraw, e.g., line 313):
Runner::run("all", false, true);

// AFTER:
Runner::pending_dirty.mark(DirtyAll | DirtyBit::ForceFullEmit);
Runner::run("all", false);
```

Note on input call sites: The 4 single-box input handlers (proc, cpu, mem, net at lines 578, 609, 631, 680) each have a local `force_redraw` bool. These should mark the specific DirtyBit for their box. The `DirtyBit::ForceFullEmit` should NOT be set for input-driven single-box redraws -- these are content redraws, not full screen repaints.

### btop_menu.cpp Call Site Migration
```cpp
// BEFORE (e.g., line 1653):
Runner::run("all", false, true);

// AFTER:
Runner::pending_dirty.mark(DirtyAll | DirtyBit::ForceFullEmit);
Runner::run("all", false);
```

### Visibility of pending_dirty
```cpp
// In btop.cpp Runner namespace (currently private):
static PendingDirty pending_dirty;  // replaces static atomic<bool> pending_redraw

// Need to expose for callers in btop_input.cpp and btop_menu.cpp.
// Option A: Add to Runner namespace in btop_shared.hpp (like request_redraw()):
namespace Runner {
    void mark_dirty(DirtyBit bits) noexcept;
}
// Implemented in btop.cpp as:
void mark_dirty(DirtyBit bits) noexcept {
    pending_dirty.mark(bits);
}

// Option B: Make pending_dirty extern.
// Option A is preferred -- keeps PendingDirty internal, exposes only the operation.
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Single atomic\<bool\> pending_redraw | PendingDirty with per-box bits | v1.6 Phase 32 | Enables per-box dirty tracking, separates ForceFullEmit |
| Single force_redraw bool for all boxes | Per-box dirty bits derived from take() | v1.6 Phase 32 | Each box can be independently dirtied |
| force_redraw drives both draw() AND full_emit | ForceFullEmit separated from per-box bits | v1.6 Phase 32 | Key presses use differential emit, resize uses full emit |

## Open Questions

1. **Should pending_dirty be exposed directly or via a wrapper function?**
   - What we know: `pending_redraw` is `static` in btop.cpp. External callers use `request_redraw()`. Input handlers in btop_input.cpp call `Runner::run()` with force_redraw=true.
   - What's unclear: Whether to expose `pending_dirty` as extern or wrap in `mark_dirty()`.
   - Recommendation: Use `mark_dirty(DirtyBit)` wrapper function declared in btop_shared.hpp, matching the existing `request_redraw()` pattern. This keeps `PendingDirty` as an internal implementation detail. `request_redraw()` can then be defined as `mark_dirty(DirtyAll | DirtyBit::ForceFullEmit)`.

2. **Should calcSizes() call sites mark ForceFullEmit?**
   - What we know: calcSizes() is called after resize and box toggle. It calls `request_redraw()` which currently sets `pending_redraw=true`. In the new system, resize needs full repaint.
   - What's unclear: Whether ALL calcSizes() callers need ForceFullEmit or just resize-triggered ones.
   - Recommendation: Yes, `request_redraw()` should mark `DirtyAll | DirtyBit::ForceFullEmit` since calcSizes() means layout changed. Phase 33 (DECPL-03) will refine this further.

3. **Normal timer-driven Runner::run("all") -- should it mark any dirty bits?**
   - What we know: btop.cpp:1556 calls `Runner::run("all")` with no force_redraw. This is the normal periodic update.
   - What's unclear: Whether the normal update path needs dirty bits at all.
   - Recommendation: The normal update path does NOT need to mark dirty bits. The draw functions' internal `redraw` namespace bools handle their own state. Dirty bits are for forced redraws from external events. The thread loop should still call `take()` to drain any accumulated bits (from concurrent callers).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build-test && ctest --test-dir . --output-on-failure` |
| Full suite command | `cd build-test && ctest --test-dir . --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| WIRE-01 | pending_redraw replaced by PendingDirty | build + grep | `cmake --build build-test` + grep for `pending_redraw` | N/A (compile check) |
| WIRE-02 | runner_conf::force_redraw removed | build | `cmake --build build-test` | N/A (compile check) |
| WIRE-03 | Draw functions receive per-box dirty state | build + existing tests | `ctest --output-on-failure` | Existing draw tests |
| WIRE-04 | ForceFullEmit drives screen_buffer.set_force_full() | unit | `ctest -R ScreenBuffer --output-on-failure` | tests/test_screen_buffer.cpp (partial) |

### Sampling Rate
- **Per task commit:** `cd build-test && cmake --build . && ctest --output-on-failure`
- **Per wave merge:** Full test suite + TSan build
- **Phase gate:** All 330+ tests pass; TSan clean

### Wave 0 Gaps
None -- existing test infrastructure covers all phase requirements. The changes are structural (compile-verified) rather than behavioral. The existing DirtyBit tests from Phase 31 verify the underlying mechanism. ScreenBuffer tests verify full_emit/diff_emit behavior.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection: btop.cpp (Runner namespace, lines 424-915), btop_shared.hpp (Runner declarations), btop_input.cpp (call sites), btop_menu.cpp (call sites), btop_draw.cpp (draw functions, calcSizes)
- btop_dirty.hpp -- Phase 31 implementation (DirtyBit enum, PendingDirty struct)
- Phase 31 research (.planning/phases/31-dirty-flags-foundation/31-RESEARCH.md)
- .planning/REQUIREMENTS.md -- WIRE-01 through WIRE-04 definitions
- .planning/STATE.md -- locked decisions (atomic<uint32_t>, fetch_or/exchange, release/acquire)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new dependencies, uses existing btop_dirty.hpp
- Architecture: HIGH - all code paths inspected, all call sites enumerated
- Pitfalls: HIGH - based on direct code inspection with specific line numbers

**Research date:** 2026-03-07
**Valid until:** 2026-04-07 (stable domain, no external dependencies)
