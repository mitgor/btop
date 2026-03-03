# Feature Research: Unified Dirty-Flag Mechanism

**Domain:** Dirty-flag / invalidation system for btop++ C++ terminal monitor
**Researched:** 2026-03-03
**Confidence:** HIGH (deep codebase analysis + established patterns from game programming, GUI frameworks, and terminal UI literature)

---

## Context: What Exists and What Changes

### Existing redraw flag topology (v1.5 baseline)

| Flag | Location | Type | Who Sets | Who Reads |
|------|----------|------|----------|-----------|
| `Cpu::redraw` | btop_draw.cpp (namespace-static) | `bool` | `Draw::calcSizes()`, `force_redraw` path | `Cpu::draw()` |
| `Mem::redraw` | btop_draw.cpp (namespace-static) | `bool` | `Draw::calcSizes()`, `force_redraw` path, disk-mount changes | `Mem::draw()` |
| `Net::redraw` | btop_draw.cpp (namespace-static) | `bool` | `Draw::calcSizes()`, `force_redraw` path, interface changes | `Net::draw()` |
| `Proc::redraw` | btop_draw.cpp (namespace-static) | `bool` | `Draw::calcSizes()`, `force_redraw` path, sort changes | `Proc::draw()` |
| `Gpu::redraw` | btop_draw.cpp (namespace-static) | `vector<bool>` | `Draw::calcSizes()`, `force_redraw` path | `Gpu::draw()` per panel |
| `Menu::redraw` | btop_menu.cpp | `bool` | `Draw::calcSizes()` when menu active | `Menu::process()` |
| `conf.force_redraw` | Runner local (RunConf struct) | `bool` | `Runner::run()` call sites, pending_redraw flush | All `::draw()` calls |
| `pending_redraw` | btop.cpp (function-static atomic) | `atomic<bool>` | `Runner::request_redraw()` | Folded into `conf.force_redraw` at run() entry |
| `Proc::resized` | btop_draw.cpp | `atomic<bool>` | **Never written** (dead) | `Draw::calcSizes()` line 2926 |
| `redraw` locals (btop_input.cpp) | btop_input.cpp, 4 separate scopes | `bool` | Input handlers per box | Passed as `force_redraw` to `Runner::run()` |

### Key problems the current topology creates

1. **Scattered mutation points.** Seven distinct module-level bools, one atomic bool, one vector of bools. No single place to inspect "what needs redraw right now."
2. **Force_redraw is all-or-nothing.** A single bool broadcasts to all boxes. There is no way to say "Mem changed its disk list; only Mem needs a redraw." Every change forces full redraw of all boxes in the cycle.
3. **`Draw::calcSizes()` sets redraw as a side-effect.** The function recomputes layout geometry (its core job) and also mass-sets every box's `redraw = true`. Layout recomputation and dirty marking are coupled in one call.
4. **`Proc::resized` is dead code.** Declared as `atomic<bool>`, never written, read once in `calcSizes()` on line 2926. The read always sees `false` — the branch is permanently taken but the flag is meaningless.
5. **Naming collision in btop_input.cpp.** Four separate `bool redraw = true;` locals in different block scopes within the same file. These shadow one another and are passed positionally to `Runner::run()` — error-prone and hard to trace.

### v1.6 target

- `DirtyFlags` bitset (one bit per box/panel) replaces all per-box bools and `force_redraw`
- Dead `Proc::resized` atomic removed
- `bool redraw` locals in btop_input.cpp renamed to meaningful identifiers
- `Draw::calcSizes()` marks DirtyFlags dirty; does not set box-level redraw bools directly

---

## Feature Landscape

### Table Stakes (Must Have for This Milestone)

Features that the unified dirty-flag mechanism must provide. Missing any = the refactor is incomplete or creates worse problems than the current code.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Single `DirtyFlags` bitset replacing all per-box redraw bools | Core goal of milestone; eliminates 7 scattered bools + vector<bool> | MEDIUM | `enum class DirtyBit : uint8_t { Cpu=0, Mem=1, Net=2, Proc=3, Gpu0..N, Menu }` + `std::bitset<N>` or `uint16_t` mask. Must cover existing flag set. |
| `set(DirtyBit)` / `test(DirtyBit)` / `clear(DirtyBit)` / `set_all()` / `clear_all()` API | All current flag operations must be expressible; no regressions in what currently triggers redraws | LOW | Thin wrapper or free functions over the bitset. `set_all()` replaces the calcSizes mass-assign. |
| `force_redraw` collapses into `set_all()` | The `RunConf::force_redraw` bool causes every draw call to see "redraw=true". After migration, this is `DirtyFlags::set_all()` before the draw cycle. | LOW | Removes `conf.force_redraw` parameter thread; replace with `dirty.test(box_bit)` in each draw call. |
| `pending_redraw` atomic folds into `DirtyFlags` | Currently `pending_redraw` is exchanged into `conf.force_redraw` at run() entry. After migration, request_redraw() sets `DirtyFlags::set_all()` atomically. | LOW | May require the bitset to be atomic or protected by the existing run-start lock. |
| `Draw::calcSizes()` calls `dirty.set_all()` instead of direct bool assigns | Line 2938 currently: `Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true`. After migration: `dirty.set_all()`. | LOW | One-line change at the call site; no behavioral change. |
| Dead `Proc::resized` removed | Declared `atomic<bool>`, never written, read once in calcSizes — the branch it guards is always taken (value is always false). No behavior change on removal. | LOW | Remove declaration from btop_shared.hpp line 721, remove read from btop_draw.cpp line 2926, simplify the condition. |
| `bool redraw` locals in btop_input.cpp renamed | Four shadowing `bool redraw = true` locals in separate block scopes in btop_input.cpp (lines 355, 587, 618, 640). These are local intent flags that get passed to `Runner::run()` as `force_redraw`. Rename to `force_box_redraw` or similar to remove collision. | LOW | Pure rename, no logic change. Compiler would have warned about this if they were in the same scope. |
| Box-draw functions receive dirty bit, not bool parameter | `Cpu::draw(cpu, force_redraw, no_update)` signature; `force_redraw` is currently a `bool`. After migration, the runner passes `dirty.test(DirtyBit::Cpu)`. Signature stays `bool` at call site or changes to direct bitset test — either is fine as long as the bool source changes. | LOW | Call-site change in Runner body (btop.cpp lines 627–712); draw function signatures may stay as `bool` internally. |
| `calcSizes()` decoupled: layout recomputation separated from dirty marking | Currently `calcSizes()` both recalculates box geometry and sets `redraw=true` on every box. After migration, `calcSizes()` does layout, then explicitly calls `dirty.set_all()`. The separation is a naming/coupling improvement — it makes clear that "recalculate layout" and "request redraw" are two distinct effects. | LOW | The separation is already almost there; it's a refactor of what currently happens on line 2938, not a functional split. |
| Existing test suite stays at 330/330 | All box behaviors must be identical. Redraw triggers must fire for exactly the same conditions as before. | HIGH (validation) | ASan + UBSan + TSan clean; no new flicker or missed redraws. |

### Differentiators (Architectural Improvements Enabled by Unified Flags)

Features that the unified bitset makes possible or materially cleaner — not strictly required for behavior parity, but they are the architectural payoff of the refactor.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Per-box selective redraw (not all-or-nothing) | Today `force_redraw` is global. With per-bit flags, a disk mount change sets only `DirtyBit::Mem`; the Cpu/Net/Proc boxes skip their static redraw path. Reduces wasted string-building on unchanged boxes. | LOW | Falls out of the design; no extra code needed. The value depends on how often partial invalidation actually occurs in practice. |
| Inspectable invalidation state for debugging | `dirty.to_string()` or a debug-log call shows exactly which bits are set at any point. Currently you must instrument 7 separate bools across 5 namespaces. | LOW | One-liner with `std::bitset::to_string()` or formatted bit print. |
| Single atomic DirtyFlags for thread-safe signaling | `pending_redraw` is currently a separate `atomic<bool>` used by `request_redraw()` to post a "please redraw" from the main thread to the runner. A `std::atomic<uint16_t>` underlying the dirty bitset allows the same operation atomically per bit. This eliminates the two-step (pending_redraw → fold into force_redraw). | MEDIUM | Requires deciding whether the bitset uses an atomic underlying word or a mutex guard at run() entry. The current code already folds at run() entry (holding the runner lock), so a non-atomic bitset behind the existing lock is valid and simpler. |
| No invalid "all boxes dirty from unrelated cause" pattern | The current `force_redraw = true` global broadcasts to all boxes even when only one triggered. With per-bit flags, only the boxes that actually changed fire their static redraw paths. Less terminal output per cycle when changes are localized. | LOW | Behavioral improvement; not a regression. Worth documenting as motivating value. |
| Compiler-visible single type for all redraw state | Today: 7 bools + 1 vector<bool> + 1 atomic<bool> + 1 bool in RunConf + 4 local bools. After: 1 type with a clear API. Grep for `DirtyFlags` finds every mutation point. | LOW | Quality-of-life for future maintainers. |

### Anti-Features (Do Not Build)

Features that seem natural additions to a dirty-flag system but are out of scope, add risk, or solve a problem btop does not have.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Sub-box dirty regions (line-level or cell-level) | Rendering engines often track dirty rects for partial repaints within a component | btop already has cell-level differential rendering in `ScreenBuffer` — the cell buffer does not emit cells that have not changed. Sub-box dirty tracking would be redundant overhead layered above a mechanism that already handles it at cell granularity. | Leave sub-box invalidation to the existing ScreenBuffer diff; DirtyFlags only tracks box-level redraw of static elements (box outlines, headers, legend that do not change every frame). |
| Dirty flag propagation hierarchy (parent → child) | GUI frameworks propagate dirtiness from parent containers to children | btop's boxes are flat peers with no parent-child containment hierarchy. There is no propagation path to design. | Not applicable to btop's layout model. |
| Lazy invalidation / on-demand evaluation | Some dirty flag systems defer recomputation until the derived value is actually requested | btop's Runner thread collects then immediately draws. There is no "deferred evaluation" call site — draw always follows collect in the same cycle. | Not needed; btop is already demand-driven per Runner cycle. |
| Async dirty flag consumers | Multiple threads watching for specific flag combinations | Runner is the only consumer of dirty state. Main thread sets flags; runner reads and clears. Multi-consumer would require condition variables and broadcast, adding complexity for no benefit. | Keep single-consumer model (Runner reads dirty bits at start of draw phase). |
| Fine-grained GPU panel dirty bits (one bit per GPU) | Gpu::redraw is currently a `vector<bool>` indexed by GPU panel index | The number of GPU panels is runtime-determined (0–N). A compile-time bitset cannot accommodate a variable number of GPUs without reserving a fixed maximum (e.g., 8 bits for up to 8 GPUs). This is workable but adds complexity. | Either reserve a fixed number of GPU bits in the bitset (e.g., 4 or 8 GPU slots), or treat all GPU panels as a single `DirtyBit::Gpu` that maps to `redraw[index] = true` for all panels when set. The latter is simpler and matches how `force_redraw` already works today — it sets all panels dirty. |
| External notification of dirty state (observer/signal) | Observer pattern lets subscribers react to flag changes | btop has no subscribers other than the Runner. Adding observer infrastructure for one consumer adds indirection with no benefit. | Direct API call (`dirty.set(bit)`) is sufficient. |
| Replacing `ScreenBuffer` differential rendering with dirty flags | A coarser dirty system could skip the cell buffer entirely for "nothing changed" frames | The cell buffer is the correct tool for cell-level unchanged-frame detection. DirtyFlags operates at a higher level (should the box static chrome be redrawn this cycle?). They are complementary, not alternatives. | Keep both: DirtyFlags for box-level invalidation, ScreenBuffer for cell-level diffing. |
| New dependency for bitset (e.g., Boost.DynamicBitset) | `std::bitset<N>` requires compile-time N; dynamic GPU count looks like it needs runtime N | PROJECT.md: no new heavy dependencies. `std::bitset<16>` with reserved GPU slots covers the realistic case. Alternatively, `uint16_t` with bit operations is zero-dependency and sufficient for < 16 boxes. | Use `uint16_t` underlying type or `std::bitset<16>` with a fixed enum, no new dependency. |

---

## Feature Dependencies

```
[DirtyFlags bitset type]
    +-- required by  --> [set/test/clear/set_all/clear_all API]
    +-- replaces     --> [Cpu::redraw, Mem::redraw, Net::redraw, Proc::redraw bools]
    +-- replaces     --> [Gpu::redraw vector<bool>]
    +-- replaces     --> [conf.force_redraw bool in RunConf]
    +-- replaces     --> [pending_redraw atomic<bool> (fold into set_all)]
    +-- enables      --> [per-box selective redraw]
    +-- enables      --> [inspectable invalidation state]

[set_all() replaces force_redraw]
    +-- requires     --> [DirtyFlags bitset type]
    +-- requires     --> [Runner draw path reads dirty.test(box_bit) not conf.force_redraw]
    +-- coordinates  --> [pending_redraw → request_redraw() calls dirty.set_all()]

[Draw::calcSizes() decoupled from redraw marking]
    +-- requires     --> [DirtyFlags bitset type]  (set_all() exists)
    +-- changes      --> [btop_draw.cpp line 2938: direct bool assigns → dirty.set_all()]
    +-- no behavior change, pure restructuring]

[Remove dead Proc::resized]
    +-- independent of --> [DirtyFlags bitset type]  (can be done in any order)
    +-- removes      --> [btop_shared.hpp atomic<bool> resized declaration]
    +-- removes      --> [btop_draw.cpp calcSizes() line 2926 condition read]
    +-- simplifies   --> [calcSizes() branch that is always taken]

[Rename redraw locals in btop_input.cpp]
    +-- independent of --> [DirtyFlags bitset type]
    +-- independent of --> [Remove dead Proc::resized]
    +-- clarifies    --> [force_box_redraw intent vs module-level redraw flags]
    +-- risk         --> LOW (pure rename, no logic change)

[Box draw functions receive dirty bit value]
    +-- requires     --> [DirtyFlags bitset type]
    +-- changes      --> [Runner body: conf.force_redraw → dirty.test(box_bit) at each draw call site]
    +-- box draw function signatures may stay bool internally]

[ScreenBuffer differential rendering] (existing, unchanged)
    +-- complements  --> [DirtyFlags bitset]  (operates at cell level below box level)
    +-- not replaced by DirtyFlags]
```

### Dependency Notes

- **DirtyFlags must be defined before any call sites are migrated.** The enum and bitset type can be introduced in `btop_shared.hpp` or a new `btop_dirty.hpp` header. All box draw call sites in btop.cpp migrate once the type exists.
- **`Proc::resized` removal is independent.** It can be a standalone commit before or after the DirtyFlags introduction. Removing it first simplifies the calcSizes() migration (one less condition to reason about).
- **Renaming btop_input.cpp locals is also independent.** Pure rename; can be first commit to clear namespace confusion, or last commit as cleanup. No ordering requirement with DirtyFlags.
- **`pending_redraw` → `request_redraw()` mapping.** Currently `request_redraw()` sets an `atomic<bool>` which is then folded into `conf.force_redraw` at run() entry. After migration, `request_redraw()` calls `dirty.set_all()`. The fold step disappears. If DirtyFlags is not atomic, `request_redraw()` must still coordinate through the runner lock or a small atomic "needs_redraw" signal that sets_all at lock acquisition. The simplest approach: keep an `atomic<bool> pending_redraw` as the cross-thread signal; fold it into `dirty.set_all()` at run() entry (same as today, just calls set_all instead of setting a bool in RunConf).
- **GPU panel granularity decision must be made early.** If one `DirtyBit::Gpu` covers all panels (simplest), the `Gpu::redraw vector<bool>` per-panel logic collapses to: when `DirtyBit::Gpu` is set, set all `redraw[i] = true`. If N GPU bits are reserved, the vector<bool> is removed and each panel tests its own bit. Either is valid; the simpler one-bit approach matches how force_redraw already works.

---

## MVP Definition

This is a refactoring milestone, not a feature-addition milestone. The MVP matches existing user-visible behavior with internal representation changed.

### Core DirtyFlags (v1.6 launch)

- [ ] `DirtyBit` enum and `DirtyFlags` type defined in a shared header
- [ ] `set(DirtyBit)` / `test(DirtyBit)` / `clear(DirtyBit)` / `set_all()` / `clear_all()` implemented
- [ ] `Cpu::redraw`, `Mem::redraw`, `Net::redraw`, `Proc::redraw` bools removed; replaced by `dirty.test(DirtyBit::Cpu)` etc. at draw call sites
- [ ] `Gpu::redraw vector<bool>` replaced (either one `DirtyBit::Gpu` or N GPU bits)
- [ ] `conf.force_redraw` bool in RunConf removed; replaced by `dirty.set_all()` at the relevant call sites
- [ ] `pending_redraw` → `request_redraw()` flows into `dirty.set_all()` (with appropriate thread safety)
- [ ] `Draw::calcSizes()` uses `dirty.set_all()` instead of line-2938 bool assigns
- [ ] All `Runner` draw call sites in btop.cpp read `dirty.test(box_bit)` instead of `conf.force_redraw`

### Dead Code Removal (v1.6 launch)

- [ ] `Proc::resized atomic<bool>` declaration removed from btop_shared.hpp
- [ ] Read of `Proc::resized` in `calcSizes()` removed from btop_draw.cpp line 2926
- [ ] Branch condition simplified (the always-taken path becomes unconditional)

### Naming Collision Fix (v1.6 launch)

- [ ] Four `bool redraw` locals in btop_input.cpp renamed to non-colliding identifiers
- [ ] No logic change; pure rename for clarity

### Validation

- [ ] All 330 existing tests pass after migration
- [ ] ASan + UBSan + TSan clean builds pass
- [ ] No visual regression: same boxes draw, same static chrome redraws on same triggers
- [ ] No new flicker (missed dirty clear) or stale display (missed dirty set)

### Deferred (v1.7+)

- [ ] Per-GPU panel bits (if 1-bit Gpu approach is chosen for v1.6, per-panel bits can follow if needed)
- [ ] Async DirtyFlags observable for a hypothetical future multi-consumer (not needed in btop)
- [ ] DirtyFlags integration with profiling / debug display

---

## Feature Prioritization Matrix

| Feature | Architectural Value | Implementation Cost | Regression Risk | Priority |
|---------|---------------------|---------------------|-----------------|----------|
| DirtyFlags bitset type + API | HIGH (foundation) | LOW | LOW | P1 |
| Replace per-box redraw bools | HIGH (core goal) | LOW | MEDIUM | P1 |
| Collapse force_redraw into set_all() | HIGH (eliminates split responsibility) | LOW | MEDIUM | P1 |
| calcSizes() decoupled from bool assigns | MEDIUM (clarity) | LOW | LOW | P1 |
| Remove dead Proc::resized | MEDIUM (eliminates dead code) | LOW | LOW | P1 |
| Rename btop_input.cpp redraw locals | MEDIUM (eliminates naming collision) | LOW | LOW | P1 |
| pending_redraw → dirty.set_all() path | MEDIUM (removes fold step) | LOW | LOW | P1 |
| Per-box selective redraw (only dirty boxes redraw static chrome) | MEDIUM (reduces wasted string-build) | LOW (falls out of design) | LOW | P1 (free side-effect) |
| Inspectable dirty state for debugging | LOW | LOW | LOW | P2 |
| Per-GPU panel bits (N bits vs 1 bit) | LOW (currently 1-bit suffices) | MEDIUM | LOW | P3 |

**Priority key:**
- P1: Required for v1.6 — refactor is not done without these
- P2: Cleanup / debug aid; add within same milestone after P1 validated
- P3: Defer to future milestone if needed at all

---

## Granularity Analysis

### Box-level granularity (recommended)

One dirty bit per box (Cpu, Mem, Net, Proc, Gpu). This matches the existing `bool redraw` per namespace: the redraw bool gates rendering of static chrome (box outlines, headers, graph init) that does not change every data cycle. Cell-level changes are handled by the ScreenBuffer diffing layer below.

**Why box-level is correct here:**
- The existing per-box `bool redraw` already expresses exactly this granularity. The migration is replacing the type, not changing the granularity.
- Sub-box dirty tracking would be redundant: the ScreenBuffer cell buffer already suppresses unchanged cells at output time.
- btop boxes have no internal sub-regions that need selective redraw — the "static chrome" vs "dynamic data" split is binary per box.

### Sub-box granularity (wrong level for this problem)

Tracking dirty regions within a box (e.g., "only the graph area in Cpu needs redraw, not the header") would require btop's draw functions to accept a dirty region parameter and conditionally skip rendering sub-sections. This is significantly more complex and provides no benefit given that the cell buffer already ensures unchanged cells are not emitted.

### All-at-once granularity (existing force_redraw — too coarse)

The current `force_redraw` bool is the opposite problem: it broadcasts a full redraw to all boxes simultaneously. Per-box bits solve this without adding sub-box complexity.

---

## Coalescing and Flush Patterns

The dirty-flag system must handle two common concurrent-access patterns:

### Pattern 1: Multiple set() calls before one flush

Multiple events in a single runner cycle may independently call `set(DirtyBit::Mem)` (e.g., a disk mount change and a config reload both trigger Mem dirty). The bitset OR-accumulates naturally — setting an already-set bit is idempotent. No explicit coalescing logic is needed.

### Pattern 2: Cross-thread request_redraw()

`Runner::request_redraw()` is called from the main thread while the runner thread may be drawing. The current solution uses `atomic<bool> pending_redraw` as a cross-thread signal, then folds it into `conf.force_redraw` at run() entry (under the runner mutex). After migration, the fold step changes from `conf.force_redraw = true` to `dirty.set_all()`. The existing lock at run() entry provides the necessary synchronization — no new locking is required.

### Pattern 3: Dirty clear after draw

Each draw function currently clears its own `redraw = false` at the end of drawing. After migration, the runner clears `dirty.clear(box_bit)` after calling each box's draw function, or clears `dirty.clear_all()` at end of the draw phase. The latter is simpler and matches the all-or-nothing nature of the current runner loop.

---

## Implementation Notes (from Codebase Analysis)

### Current redraw trigger inventory

The following conditions currently set per-box `redraw = true` beyond the `force_redraw` path. These must remain correct after migration (i.e., `dirty.set(DirtyBit::Box)` must be called in the same conditions):

| Trigger | Box | Location |
|---------|-----|----------|
| `Draw::calcSizes()` (resize / layout change) | ALL | btop_draw.cpp:2938 |
| New disk mount found | Mem | linux/btop_collect.cpp:2315, 2368 |
| Disk set changed | Mem | linux/btop_collect.cpp:2826, 2865 |
| Network interface change | Net | linux/btop_collect.cpp:2700, 2974, 3370 |
| More CPU cores detected than expected | Cpu | linux/btop_collect.cpp:1204 (posts Resize event) |
| Proc sort order or filter changes | Proc | btop_draw.cpp:2247, 2357, 2361 |
| Menu active on calcSizes | Menu | btop_draw.cpp:2930 |
| `force_redraw` passed to each draw() | ALL | btop.cpp:627–712 |

The macOS, FreeBSD, NetBSD, OpenBSD collect paths also set `redraw = true` in their collector namespaces (confirmed by grep hits in osx/btop_collect.cpp, freebsd/btop_collect.cpp, netbsd/btop_collect.cpp, openbsd/btop_collect.cpp). Each of these will need `dirty.set(DirtyBit::Net)` or `dirty.set(DirtyBit::Mem)` substituted.

### DirtyBit enum sketch

```cpp
enum class DirtyBit : uint16_t {
    Cpu  = 1 << 0,
    Mem  = 1 << 1,
    Net  = 1 << 2,
    Proc = 1 << 3,
    Gpu  = 1 << 4,   // covers all GPU panels (simplest)
    Menu = 1 << 5,
    All  = 0xFFFF,
};

struct DirtyFlags {
    uint16_t bits = 0;
    void set(DirtyBit b)  noexcept { bits |= static_cast<uint16_t>(b); }
    void clear(DirtyBit b) noexcept { bits &= ~static_cast<uint16_t>(b); }
    bool test(DirtyBit b) const noexcept { return (bits & static_cast<uint16_t>(b)) != 0; }
    void set_all()   noexcept { bits = 0xFFFF; }
    void clear_all() noexcept { bits = 0; }
    bool any() const noexcept { return bits != 0; }
};
```

This is zero-dependency, trivially copyable, and fits in a uint16_t (no heap allocation). Alternatively, `std::bitset<16>` with the same enum works and gives `to_string()` for debugging at the cost of slightly more verbose bitwise ops.

### Proposed DirtyFlags location

Two options:
1. In `btop_shared.hpp` as a global (matches where `Proc::redraw` and other shared state live).
2. In a new `btop_dirty.hpp` header included by btop.cpp and btop_draw.cpp.

Option 1 is simpler — it follows the pattern established by all other shared btop state.

---

## Sources

- Codebase analysis: `btop.cpp`, `btop_draw.cpp`, `btop_shared.hpp`, `btop_input.cpp`, `btop_state.hpp`, `linux/btop_collect.cpp`, `osx/btop_collect.cpp`, `freebsd/btop_collect.cpp`, `netbsd/btop_collect.cpp`, `openbsd/btop_collect.cpp`
- [Game Programming Patterns: Dirty Flag](https://gameprogrammingpatterns.com/dirty-flag.html) — canonical pattern: granularity spectrum, on-demand vs checkpoint vs background clearing, propagation pitfalls, encapsulate mutations behind narrow API
- [Mozilla Bug 1699603: Switch dirty region tracking from bitmask to rects](https://bugzilla.mozilla.org/show_bug.cgi?id=1699603) — real-world bitmask granularity overflow pitfall in a rendering engine
- [Qt QWidget update() vs repaint() — Qt 6 docs](https://doc.qt.io/qt-6/qwidget.html) — coalescing multiple update() calls into one paintEvent; Qt's `setDirtyOpaqueRegion()` internal dirty-region accumulation pattern
- [Win32 InvalidateRect / GetUpdateRect — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-invalidaterect) — invalidate region accumulation, QS_PAINT deferred coalescing, UnionRect for dirty accumulation
- [Ratatui: Rendering concepts](https://ratatui.rs/concepts/rendering/) — immediate mode redraws every frame; buffer diff at terminal output layer handles unchanged cells (analogous to btop's ScreenBuffer)
- [Type-safe Enum Class Bit Flags](https://voithos.io/articles/type-safe-enum-class-bit-flags/) — `enum class` with bitwise operators for type-safe per-component flags in C++
- `.planning/PROJECT.md` — v1.6 scope, constraints, and specific requirements

---
*Feature research for: btop++ v1.6 Unified Dirty-Flag Mechanism*
*Researched: 2026-03-03*
