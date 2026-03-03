# Pitfalls Research

**Domain:** C++ terminal UI — unified dirty-flag consolidation added to existing multi-path rendering pipeline (btop++ v1.6)
**Researched:** 2026-03-03
**Confidence:** HIGH — based on direct btop++ source analysis (five per-box redraw bools, force_redraw per-cycle field, pending_redraw atomic, ScreenBuffer::force_full latch, Menu::redraw, dead Proc::resized), established literature on dirty-flag patterns (Game Programming Patterns), and lessons from v1.1/v1.2 shadow-atomic desync experience

---

## Critical Pitfalls

### Pitfall 1: Atomicity Gap During Migration — Dual-Write Window Between Old Flags and New Bitset

**What goes wrong:**
The migration from five per-box `bool redraw` globals (`Cpu::redraw`, `Mem::redraw`, `Net::redraw`, `Proc::redraw`, `Gpu::redraw[i]`) plus `runner_conf::force_redraw` to a unified `DirtyFlags` bitset will be incremental. During the transition, any call site that sets the old bool but does not set the corresponding bitset bit — or vice versa — creates a window where the flag system is inconsistent.

This is exactly the v1.2 bug pattern: `runner_var` (the variant) was stale because all writes went through the shadow atomic, leaving the variant dead. Here the reverse happens: code that has been migrated reads from `DirtyFlags`, while not-yet-migrated callers still write to the per-box bools. The runner, reading only `DirtyFlags`, sees "no redraw requested" even though `Proc::redraw` is `true`.

Concrete example: `Runner::run()` passes `force_redraw` into `runner_conf`, which each `draw()` function checks. If `Proc::draw()` is migrated to read from `DirtyFlags::Proc` but `btop_input.cpp` still writes `Proc::redraw = true` (the old path), the redraw never fires under the new system.

**Why it happens:**
There are at least 20 write sites across `btop_input.cpp`, `btop_menu.cpp`, `btop_draw.cpp` (in `calcSizes()`), and `btop.cpp` (in `on_enter(Resizing)` and `on_enter(Reloading)`). Migrating the reader before all writers, or migrating writers in batches without removing old writes in the same commit, creates a temporary state where both systems exist but only one is consulted.

**How to avoid:**
- Define the `DirtyFlags` enum/bitset type and its write API (e.g., `DirtyFlags::mark(Box)`) before migrating any read site.
- Migrate writes and reads for one box at a time within a single atomic commit. Never leave a partial migration where a box's write path uses the old bool but the read path uses `DirtyFlags`.
- Immediately after each box is migrated, delete the old `bool redraw` declaration from the box namespace. Compile errors from remaining old-style write sites are the migration checklist.
- Do not keep both the old bool and the new bitset bit alive in parallel longer than one plan. The v1.2 retrospective: "shadow atomics create consistency debt" — dual representation is a bug factory.

**Warning signs:**
- A box that should redraw (e.g., after pressing a key that sets `Proc::redraw = true`) does not redraw after migration.
- TSan reports no data race (both paths are on the runner thread), but the behavior is silently wrong — the flag was set on the old path and read on the new path.
- `redraw = true` assignments remain in files after the per-box bool declaration has been removed — these cause compile errors that catch the problem before it runs.

**Phase to address:**
First implementation phase (DirtyFlags type definition). The write API must be finalized and the deletion-on-migrate rule established before any per-box bool is touched.

---

### Pitfall 2: Under-Invalidation — Missing Write Sites Produce Stale Frames

**What goes wrong:**
The system currently works correctly. The goal is architectural consolidation, not bug fixes. Every existing write to a per-box `redraw` bool represents a legitimate and necessary invalidation. If any write site is missed during migration — left out of the new `DirtyFlags::mark()` call — the corresponding box silently stops invalidating when that trigger fires.

The risk is not obvious because the box still redraws on resize (handled by `calcSizes()` which does a bulk `Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true` at line 2938 of `btop_draw.cpp`). Resize-triggered redraws mask the missing invalidation. The regression only surfaces when the specific trigger (e.g., an interface change in Net::draw that sets `redraw = true` when `old_ip != ip_addr`) fires in steady-state operation without a preceding resize.

Concrete current write sites that are easy to miss:
- `Net::draw()` sets `redraw = true` when `old_ip != ip_addr` (line 2107 in `btop_draw.cpp`) — a data-triggered self-invalidation inside the draw function itself.
- `Net::draw()` sets `redraw = true` on interface change (line 2247) — same pattern.
- `Proc::draw()` sets `redraw = true` in `selection()` when `follow_process` logic fires (line 2247 in `btop_draw.cpp`).
- Input handler local `bool redraw` variables in `btop_input.cpp` (lines 355, 587, 618, 640) — these shadow the namespace-scoped `Proc::redraw`, `Cpu::redraw`, etc. They are passed to `Runner::run(box, no_update, redraw)` as the `force_redraw` argument, not set on the per-box bool directly.

**Why it happens:**
The per-box redraw bools serve two different roles: (a) a persistent "needs redraw" flag carried across frames (the namespace-scoped `bool redraw = true` in each box), and (b) a per-invocation override passed as `force_redraw` parameter into `draw()` functions. A unified `DirtyFlags` bitset must cover both roles — or the architecture must explicitly separate them. Conflating them produces a design where some bits need to persist until consumed (like (a)) and others are cycle-ephemeral (like (b)). Missing this distinction leads to either bits that are never cleared or bits that are cleared too early.

**How to avoid:**
- Before writing any code, enumerate all write sites for each per-box bool. The authoritative source is `grep -rn "redraw\s*=" src/`. There are at least 15 distinct sites.
- Distinguish data-triggered self-invalidations (inside `draw()`) from externally-triggered invalidations (from `btop_input.cpp`, `btop_menu.cpp`, `calcSizes()`). The architecture must handle both.
- For self-invalidations inside `draw()`: the draw function must still write the flag; the only change is where the flag lives (bitset bit vs. namespace bool). Do not delete these writes.
- For the `force_redraw` parameter pattern: this is per-cycle ephemeral and can be collapsed into the bitset if the bitset is consumed-and-cleared each cycle. Ensure the clearing happens at the right point (after draw, not before).
- Test each trigger independently: resize, interface change, IP change, filter change, sort change. Do not rely only on resize tests.

**Warning signs:**
- Net box fails to redraw its header when the active network interface changes (IP address change).
- Proc box fails to redraw after sort order is toggled.
- Box appears frozen at previous visual state despite new data arriving.
- All boxes redraw correctly on resize but not on key-triggered config changes — indicates `calcSizes()` path was migrated but input handler path was not.

**Phase to address:**
Pre-implementation audit phase (write-site inventory). The full list of write sites must be enumerated and classified before any migration begins.

---

### Pitfall 3: Over-Invalidation — Bitset Granularity Too Coarse Defeats Differential Rendering

**What goes wrong:**
The current system has fine-grained per-box redraw bools. `runner_conf::force_redraw` is a separate concept: when true, it forces all boxes to redraw AND sets `screen_buffer.set_force_full()`, which bypasses the differential cell-buffer comparison and emits the full terminal frame. These are two different mechanisms with different effects:
- Per-box `redraw = true`: causes a box to regenerate its `string` output (expensive string building).
- `force_full`: causes the output emitter to bypass the cell-buffer diff and write all cells unconditionally (expensive terminal I/O).

If `DirtyFlags` conflates per-box invalidation with screen-buffer invalidation into a single bit, or if the bitset's `all-boxes` variant always triggers `force_full`, the result is that any single-box key-press (e.g., toggling proc sort order) forces a full terminal repaint — undoing the differential rendering work from v1.0.

Concrete current architecture: `runner_conf::force_redraw = true` causes both (1) all per-box `draw()` calls to receive `force_redraw = true` and (2) `screen_buffer.set_force_full()` to be called (line 737 of `btop.cpp`). These are currently tied together. The unified bitset must preserve the ability to invalidate individual boxes without triggering `force_full`.

**Why it happens:**
When consolidating flags, the temptation is to create a single "dirty" concept: if dirty → redraw everything. This is safe (no under-invalidation) but wasteful. The existing system's granularity — where toggling proc sorting redraws only the proc box, not cpu/mem/net — is a deliberate performance property. A DirtyFlags bitset with per-box bits naturally preserves this if the runner checks each bit independently. The problem arises if the "force full screen repaint" logic (currently tied to `force_redraw`) is accidentally attached to the per-box dirty path.

**How to avoid:**
- Keep the two concerns explicitly separate in the DirtyFlags design: per-box content invalidation (bits 0–N for each box) and full-screen emit (a separate flag or mechanism).
- `ScreenBuffer::force_full` should only be set when: (a) a resize event fires, (b) a reload event fires, or (c) an explicit "full repaint requested" signal. It must NOT be set for normal per-box content invalidation.
- Verify the runner loop: after migration, a dirty-proc-only cycle must call only `Proc::draw(force_redraw=true)` with the other boxes getting `force_redraw=false`, and `screen_buffer.needs_full()` must return `false` (so `diff_and_emit` is used, not `full_emit`).
- Write a benchmark or metric assertion: single-key-press redraw should not cause `full_emit`; only resize/reload should.

**Warning signs:**
- After migration, all key presses cause a visible full-terminal flicker (the tell-tale sign of `full_emit` instead of `diff_and_emit`).
- TSan or perf profiles show `write_stdout` call volume increasing significantly after migration (more bytes written per cycle).
- The `screen_buffer.needs_full()` branch is taken on cycles where no resize occurred.

**Phase to address:**
DirtyFlags API design phase. The per-box bits and the full-screen-emit flag must be specified as separate concerns before any runner-loop code is touched.

---

### Pitfall 4: Cross-Thread Ordering — `pending_redraw` Folding Loses Ordering Guarantee

**What goes wrong:**
The current system has `Runner::pending_redraw` (an `atomic<bool>`) that is set by `Runner::request_redraw()` and folded into `runner_conf::force_redraw` at the start of each run cycle (line 903 of `btop.cpp`). The folding uses `pending_redraw.exchange(false, std::memory_order_relaxed)`. This is intentionally relaxed because `force_redraw` is then carried as a struct field accessed only on the runner thread.

If the unified `DirtyFlags` bitset is made a plain `std::bitset<N>` (not atomic), and `request_redraw()` is rewritten to set a bit directly on this bitset from the main thread while the runner thread is reading it, a data race occurs. `std::bitset` provides no thread-safety guarantees.

The current `pending_redraw` atomic exists precisely because `request_redraw()` is called from the main thread (e.g., from `on_enter(Reloading&)` in `btop.cpp` line 1279 context), while the runner thread reads and processes the flag. Any migration that moves from an atomic to a plain bitset must maintain the thread-safety boundary.

**Why it happens:**
The per-box `bool redraw` variables are only written and read on the runner thread (inside `draw()` and `Runner::run()`). They are not atomic because they are thread-local in practice. The per-box bools being non-atomic is correct. But `pending_redraw` is genuinely cross-thread. If a unified bitset merges these two — per-box runner-local bits and the cross-thread request-redraw bit — without making the cross-thread bits atomic, a race is introduced.

**How to avoid:**
- Separate the design into two components: (1) `RunnerDirtyFlags` — a plain (non-atomic) bitset accessed only on the runner thread, holding per-box content-dirty bits; (2) an atomic mechanism for cross-thread "request full redraw" (either keep `pending_redraw` as-is or make the relevant bitset bits atomic).
- Alternatively: make `DirtyFlags` an `atomic<uint32_t>` and use `fetch_or` for setting bits and `exchange(0)` for consuming them. This is correct for `std::atomic<uint32_t>` when the bitset width fits in 32 bits (5 boxes + menu + force_full = 7 bits, comfortably fits).
- Run TSan on the migrated code. The existing codebase is TSan-clean; any new race introduced by the migration will be caught.
- Do not use `std::atomic<std::bitset<N>>` — `std::bitset` is not trivially copyable in all implementations; use `std::atomic<uint32_t>` with manual bit manipulation or per-bit `std::atomic<bool>` for cross-thread bits.

**Warning signs:**
- TSan reports: "data race on DirtyFlags" or "read of size N by thread T1, write by thread T2".
- Intermittent missed redraws that are not reproducible with TSan disabled (TSan slows threads, masking the race).
- The migration compiles and appears to work but the race is latent.

**Phase to address:**
DirtyFlags type design phase. Thread ownership of each bit must be documented before the type is coded. TSan-clean requirement (existing) means any race will be caught in CI.

---

### Pitfall 5: `calcSizes()` Decoupling — Layout Recomputation Silently Coupled to Redraw Forcing

**What goes wrong:**
`Draw::calcSizes()` currently does two things in one call: (1) it recomputes all box geometries (x, y, width, height) based on current terminal size and config, and (2) it sets `Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true` (line 2938 of `btop_draw.cpp`). The v1.6 goal explicitly includes decoupling these: "Decouple calcSizes() from redraw forcing."

The pitfall is decoupling them in the wrong order. If the redraw-forcing is removed from `calcSizes()` before the `DirtyFlags` mechanism reliably marks all boxes dirty after a layout change, there is a window where layout recomputes but boxes are not marked dirty — producing a frame where boxes render at new geometry coordinates but with stale content strings (which were generated at the old geometry).

Concrete risk: the caller of `calcSizes()` is responsible for marking boxes dirty. If `on_enter(Resizing)` is the expected location for this marking, but some call site (e.g., `btop_menu.cpp:1647` calling `Draw::calcSizes()` on `screen_redraw`) does not also call `DirtyFlags::mark_all()`, that particular resize path leaves boxes with stale content.

**Why it happens:**
The coupling in `calcSizes()` is a defensive design: whoever calls `calcSizes()` gets redraw forcing "for free." Removing it makes the caller responsible for marking dirty flags. With 5+ call sites for `calcSizes()` across `btop.cpp`, `btop_input.cpp`, and `btop_menu.cpp`, missing one is easy.

**How to avoid:**
- Enumerate all `calcSizes()` call sites before decoupling (there are 5: `on_enter(Resizing)`, `on_enter(Reloading)`, initialization in `main()`, two in `btop_input.cpp` for preset changes, one in `btop_menu.cpp` for screen_redraw). Each must either: (a) call `DirtyFlags::mark_all()` after `calcSizes()`, or (b) rely on a higher-level path (e.g., `on_enter(Resizing)` calls `Runner::run("all", true, force_redraw=true)` which already marks all dirty).
- Do not decouple the redraw-forcing from `calcSizes()` until the `DirtyFlags` API is complete and all call sites have been audited and updated.
- Write a test or assertion: after `calcSizes()` is called, `DirtyFlags::any()` must return `true`. Add a `[[nodiscard]]` check or a debug assertion that fires if `calcSizes()` returns without any dirty flags being set.
- The decoupling phase should be a separate, focused plan that changes only `calcSizes()` and its callers — not bundled with other flag migration work.

**Warning signs:**
- After resize, boxes render at correct new sizes but content (graphs, text) has not been regenerated — ghosting or visual artifacts from stale content at new coordinates.
- The `btop_menu.cpp:screen_redraw` path (which calls `calcSizes()` directly after options menu close) produces incorrect display after changing a layout-affecting config option.
- Tests that cover the resize path pass, but tests that cover the options-menu-layout-change path fail.

**Phase to address:**
A dedicated decoupling phase (must follow the DirtyFlags-for-all-boxes phase). Never bundled with the atomicity migration.

---

### Pitfall 6: `Menu::redraw` Semantic Mismatch — Menu Redraw Is Not a Box Dirty Flag

**What goes wrong:**
The current system has `Menu::redraw` (a plain `bool` in `btop_menu.cpp`) which tells `Menu::process()` to regenerate the overlay string. This is semantically different from the per-box redraw bools: the menu overlay is rendered as an overlay over the main boxes, not as a box itself. It does not feed into `Runner::run()`'s box loop. It is consumed by `Menu::process()` directly when input is processed.

If `DirtyFlags` is extended to include a `Menu` bit as one of the box flags, and the runner loop checks it like other boxes, the menu redraw will be processed in the wrong context (wrong thread, wrong timing). The menu overlay rendering is tied to the input event loop, not the collect/draw cycle.

Additionally, `Menu::redraw = true` is set in `calcSizes()` at line 2930 only if `Input::is_menu_active()`. This conditional set is the correct behavior. A unified bitset that sets the Menu bit unconditionally in all resize paths would trigger spurious `Menu::process()` calls when no menu is active.

**Why it happens:**
When creating a `DirtyFlags` type with one bit per "thing that needs updating," it is tempting to include `Menu` as one of the flags for uniformity. But menu rendering has a different owner, different timing, and different consumption pattern than box rendering.

**How to avoid:**
- Keep `Menu::redraw` separate from the box-level `DirtyFlags` bitset. The bitset covers: Cpu, Mem, Net, Proc, Gpu (per-panel). Menu remains its own bool.
- Document this boundary explicitly: "DirtyFlags covers Runner-thread box content. Menu::redraw is consumed by the main-thread input loop via Menu::process()."
- When migrating `calcSizes()` to use `DirtyFlags::mark_all()` for box bits, retain the conditional `if (Input::is_menu_active()) Menu::redraw = true;` line as-is. Do not fold it into the bitset.

**Warning signs:**
- Menu overlay disappears or fails to redraw after terminal resize when menu is open (indicates `Menu::redraw = true` was removed or conditional was lost).
- `Menu::process()` is called from the runner thread instead of the main thread (architecture violation; would be flagged by TSan if thread identities are checked).
- Menu redraw state leaks between cycles — a menu redraw is triggered even when no menu is open.

**Phase to address:**
DirtyFlags design phase. The Menu exclusion must be specified in the type definition before any migration begins.

---

### Pitfall 7: Dead Code Removal Race — `Proc::resized` Has Latent Read in `calcSizes()`

**What goes wrong:**
`Proc::resized` is declared as `atomic<bool>` in `btop_shared.hpp` (line 721) and in `btop_draw.cpp` (line 2220). The v1.6 plan is to remove it as dead code (it is "never written"). However, there is one read of it: `calcSizes()` at line 2926 of `btop_draw.cpp`:

```cpp
if (not (Proc::resized or Global::app_state.load() == AppStateTag::Resizing)) {
    Proc::p_counters.clear();
    Proc::p_graphs.clear();
}
```

This read guards the clearing of `p_counters` and `p_graphs`. If `Proc::resized` is removed without understanding this guard, the removal changes behavior: `p_counters` and `p_graphs` will always be cleared on non-resize `calcSizes()` calls (since `Proc::resized` was always `false`, the condition `not (false or ...)` is always `not (app_state == Resizing)`). Since `Proc::resized` was never written (always `false`), removing it and simplifying the condition to `app_state != Resizing` is semantically correct — but this analysis must be explicit, not assumed.

**Why it happens:**
"Dead code" means "never written" but the variable is still read. A read of a variable whose value is always the default (`false`) is not dead — it silently controls a branch. Removing it without simplifying the branch incorrectly preserves or incorrectly eliminates downstream behavior. The "dead" label applies to the write side only; the read side has observable effects via the branch it guards.

**How to avoid:**
- Before removing `Proc::resized`, trace every read site and determine the effect when the value is always `false`. In this case: the condition `not (Proc::resized or ...)` simplifies to `not (...)` — the `Proc::resized` term is vacuously `false` and the condition becomes `app_state != Resizing`.
- Write the simplified condition explicitly rather than just deleting the variable and hoping the compiler eliminates the dead branch.
- Add a comment explaining why the guard was simplified: "Proc::resized removed (was never written, always false); the guard now depends only on app_state."
- Test the `p_counters.clear()` / `p_graphs.clear()` path specifically after `Proc::resized` removal to verify graph continuity across non-resize `calcSizes()` calls.

**Warning signs:**
- Compilation error after removing the declaration from `btop_shared.hpp` due to remaining read in `btop_draw.cpp` — this is the correct outcome and will catch the issue before it runs.
- After removal, process graph histories reset on config changes that call `calcSizes()` but are not resize events (e.g., toggling `show_disks` in Mem box).
- Process CPU graphs show discontinuities after options are changed in menu.

**Phase to address:**
Dead code removal phase. Must be a separate, focused plan with explicit simplification of the `calcSizes()` condition documented in the plan.

---

### Pitfall 8: Naming Collision Shadowing — Local `bool redraw` Shadows Namespace `bool redraw` Silently

**What goes wrong:**
In `btop_input.cpp`, each key-handler block declares a local `bool redraw = true` (lines 355, 587, 618, 640). These are passed to `Runner::run(box, no_update, redraw)` as the `force_redraw` argument. They shadow the namespace-scoped `Proc::redraw`, `Cpu::redraw`, `Mem::redraw`, `Net::redraw` without the compiler warning (same type, different scope).

The current behavior: the local `bool redraw` is initialized to `true`, set to `false` if the key press requires no redraw, then passed to `Runner::run()` as the cycle's `force_redraw`. This is correct. The namespace-scoped `Xxx::redraw` is a separate persistent flag.

If the migration removes the namespace-scoped `bool redraw` variables (replaced by `DirtyFlags`) but leaves the local `bool redraw` declarations in `btop_input.cpp` with the same name, no compile error occurs — they still compile cleanly. However, they no longer shadow anything meaningful (the namespace bool is gone), and if a later developer adds `using namespace Cpu;` or similar, the shadowing semantics may silently re-emerge. The naming collision is also confusing for maintenance: the local `bool redraw` in the Proc key handler means "should this key press trigger a force-redraw this cycle" while the now-removed `Proc::redraw` meant "has the proc box been invalidated persistently." Two different concepts, same name.

**Why it happens:**
The v1.6 requirements explicitly call out "Fix redraw naming collisions in btop_input.cpp." The collision exists by design in the original code (the local `bool redraw` in each key handler is intentionally local). The migration must rename these locals (e.g., `bool trigger_redraw` or `bool force_this_cycle`) to prevent future confusion and to make the distinction between ephemeral per-cycle force and persistent dirty flags unambiguous.

**How to avoid:**
- Rename all four local `bool redraw` variables in `btop_input.cpp` to `bool force_redraw` (or `bool request_redraw`) at the start of the v1.6 work, as a standalone rename commit. This has zero behavioral change and eliminates the namespace collision risk.
- After renaming, the compile errors (if any) reveal any accidental `Xxx::redraw` reads that were being shadowed by the local.
- Do the rename as the very first commit of v1.6, before any flag migration, so that subsequent diffs are clean.

**Warning signs:**
- `grep -n "bool redraw" src/btop_input.cpp` still returns results after namespace `bool redraw` declarations have been removed — these are the locally-scoped ones.
- A developer reading `btop_input.cpp` after migration cannot distinguish "this `redraw` is the force-this-cycle parameter" from "this `redraw` is the persistent dirty flag" without looking at the declaration context.
- A future change that adds `using namespace Proc;` in a handler block silently makes the local shadow a now-meaningless namespace symbol.

**Phase to address:**
Phase 1 cleanup — the rename is a pure refactor with zero behavior change and should be the first commit, establishing the naming contract before any flag logic is touched.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Keep per-box `bool redraw` alongside `DirtyFlags` during migration | No big-bang change, safe fallback | Dual representation grows inconsistent; any missed sync site is a silent incorrect-redraw bug | Only for a single plan; remove the old bool in the same plan that migrates its readers |
| Use a single `force_all` bit in `DirtyFlags` instead of per-box bits | Simpler API, no granularity decisions | Every key press forces all boxes to redraw; destroys differential-rendering benefit | Never — per-box granularity is the point of the consolidation |
| Make `DirtyFlags` a plain `std::bitset<N>` (non-atomic) and protect all access with the runner lock | Simpler type | Must audit every lock site; any unlocked cross-thread access is a race | Only if all cross-thread writes go through a locked section; very hard to verify |
| Defer `Proc::resized` removal to a later milestone | Reduces scope of v1.6 | Leaves dead code indefinitely; read-of-always-false in `calcSizes()` guard remains a maintenance hazard | Acceptable if v1.6 scope is too large; document explicitly why it was deferred |
| Rename local `bool redraw` in `btop_input.cpp` as a separate unplanned cleanup | Zero semantic change | Delays elimination of naming confusion; risks naming collision surviving into post-migration codebase | Never — this is a trivial rename with no risk |

## Integration Gotchas

Common mistakes when connecting the new `DirtyFlags` to the existing btop rendering pipeline.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Runner `runner_conf::force_redraw` | Removing the struct field before migrating all call sites that pass it | Keep `force_redraw` in `runner_conf` and map it from `DirtyFlags::any()` during the transition; remove the struct field only when all per-box bits are in `DirtyFlags` |
| `ScreenBuffer::force_full` | Tying `set_force_full()` to any `DirtyFlags` bit set event | `set_force_full()` is for resize/reload events only; per-box dirty bits must not trigger it |
| `pending_redraw` atomic | Removing the atomic and using a plain `DirtyFlags` bit written cross-thread | Either keep `pending_redraw` as an atomic sentinel and fold it into `DirtyFlags` at the start of each run cycle (current pattern), or make the corresponding `DirtyFlags` bit atomic |
| `calcSizes()` call sites | Removing redraw-forcing from `calcSizes()` before `DirtyFlags` callers are updated | Decouple in a dedicated phase; audit all 5 call sites before touching `calcSizes()` |
| `Menu::redraw` | Including it as a bit in `DirtyFlags` | Keep it as a standalone `bool`; it is consumed by the main-thread input loop, not the runner thread |
| Gpu `vector<bool> redraw` | Treating it as a simple bool replacement | `Gpu::redraw` is a vector indexed by GPU panel index; the `DirtyFlags` GPU bits must be a range of bits (one per panel) or a separate vector; cannot flatten to a single bit |

## Performance Traps

Patterns that create measurable overhead in the render path.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Using `DirtyFlags::any()` to decide `full_emit` vs `diff_and_emit` | Full terminal repaint on every key press | `full_emit` must be gated on `ScreenBuffer::needs_full()` (resize/reload), never on `DirtyFlags::any()` | Every non-resize interaction |
| Clearing all `DirtyFlags` bits at the start of each runner cycle instead of after each box draws | A box that is dirty but draws nothing (e.g., zero-size box) has its dirty bit cleared; next cycle it is not redrawn | Clear each box's bit after its `draw()` call completes successfully, not before | Any hidden box whose redraw was deferred |
| Making `DirtyFlags` an `std::atomic<std::bitset<N>>` | Compile error or undefined behavior (`std::bitset` is not trivially copyable in all implementations) | Use `std::atomic<uint32_t>` with manual bit manipulation for at most 32 flags | Build attempt |
| Rebuilding box content strings even when `force_redraw = false` and data has not changed | Increased CPU usage; negates differential rendering | The `data_same` parameter to `draw()` should suppress graph rebuilds; `force_redraw` should not imply `data_same = false` | Low-update-rate scenarios |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Write-site audit:** Often incomplete — verify `grep -rn "redraw\s*=\s*true" src/` returns zero hits for the namespace-scoped bools after migration (local variable hits are acceptable)
- [ ] **GPU redraw vector:** Often forgotten — `Gpu::redraw` is `vector<bool>`, not a single bool; verify all GPU panel indices are covered by `DirtyFlags` or a separate mechanism
- [ ] **Self-invalidating draw functions:** Often missed — `Net::draw()` sets `redraw = true` internally when IP changes; verify this logic survives migration and writes to the correct flag location
- [ ] **`calcSizes()` call sites:** Often partially updated — verify all 5 call sites mark boxes dirty after decoupling (not just the resize path)
- [ ] **`pending_redraw` fold:** Often orphaned — verify the `pending_redraw.exchange(false)` → `conf.force_redraw = true` fold still works after `DirtyFlags` is introduced; do not silently leave `pending_redraw` alive as a dead atomic
- [ ] **TSan clean:** Often skipped after refactor — run the full TSan configuration after migration; the existing codebase is TSan-clean and any regression indicates a new race
- [ ] **`Proc::resized` guard simplification:** Often treated as delete-only — verify the `calcSizes()` branch condition is explicitly simplified, not just compiled-away
- [ ] **`Menu::redraw` preserved:** Often accidentally included in bitset — verify `Menu::redraw` remains a standalone `bool` outside `DirtyFlags`

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Dual-write atomicity gap (missed write site) | LOW | `grep -rn "redraw\s*=\s*true" src/` will reveal remaining old-style write sites; add corresponding `DirtyFlags::mark()` call; add the missing call site to the write-site inventory for future reference |
| Under-invalidation (box fails to redraw) | LOW | Reproduce the trigger (specific key press or data change); trace backward from the missed redraw to the write site that should have set the flag; identify whether it was a missed migration or a deleted self-invalidation |
| Over-invalidation (full repaint on key press) | MEDIUM | Profile `write_stdout` byte count per cycle; if elevated, check whether `screen_buffer.set_force_full()` is being called from a per-box-dirty path; trace to the specific call chain that triggers it |
| Cross-thread race on DirtyFlags | HIGH | TSan report will identify the exact file/line of the racing access; convert the racing bit(s) to `std::atomic<bool>` or fold the cross-thread write through the existing `pending_redraw` atomic and consume into `DirtyFlags` at cycle start |
| `calcSizes()` decoupling regression (stale content at new geometry) | MEDIUM | Check whether the specific `calcSizes()` call site that triggered the issue calls `DirtyFlags::mark_all()` after the call; if not, add it; write a test that changes a layout config option through the options menu and verifies box content is regenerated |
| `Proc::resized` removal changes behavior | LOW | Restore the declaration; explicitly analyze the branch guard; write the simplified condition; re-remove the declaration; the branch guard must be documented in the commit message |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Dual-write atomicity gap | Phase 1: DirtyFlags type definition — establish write API and delete-on-migrate rule | Per-box bool declaration removed in the same commit as its readers are migrated; compile confirms no remaining old-style writes |
| Under-invalidation (missing write sites) | Pre-phase audit — enumerate all write sites before any code changes | Write-site inventory document reviewed in plan before migration starts |
| Over-invalidation (coarse bits defeat diff rendering) | Phase 1: DirtyFlags API design — per-box bits and force_full are explicitly separate | After migration, verify `diff_and_emit` is used for single-key-press cycles (not `full_emit`) |
| Cross-thread race on `pending_redraw` / DirtyFlags | Phase 1: Thread ownership documented per bit | TSan configuration passes after migration (existing CI requirement) |
| `calcSizes()` decoupling without marking boxes dirty | Dedicated decoupling phase (follows all-boxes migration) | All 5 call sites updated; `assert(DirtyFlags::any())` after each `calcSizes()` call in debug builds |
| `Menu::redraw` absorbed into bitset | Phase 1: DirtyFlags type spec explicitly excludes Menu | `Menu::redraw` is a standalone `bool` in final implementation; not a `DirtyFlags` bit |
| `Proc::resized` dead-read not simplified | Dead code removal phase | The `calcSizes()` guard condition is explicitly simplified in the commit, not just compiled away |
| Naming collision (local `bool redraw` in input handlers) | Phase 1 cleanup — rename as first commit of v1.6 | `grep -n "bool redraw" src/btop_input.cpp` returns zero after rename |

## Sources

- btop++ source code analysis: `btop_draw.cpp` (lines 1039, 1615, 1809, 1823, 2079, 2090, 2194, 2200, 2905–2939), `btop_input.cpp` (lines 355, 587, 618, 640), `btop.cpp` (lines 427–905, 735–820), `btop_shared.hpp` (lines 488, 575, 620, 663, 715, 721), `btop_menu.cpp` (line 62), `btop_draw.hpp` (lines 99–106, 123–126) — direct codebase inspection, HIGH confidence
- Game Programming Patterns — [Dirty Flag pattern (Robert Nystrom)](https://gameprogrammingpatterns.com/dirty-flag.html): canonical pitfalls of dirty-flag patterns — cache invalidation miss, deferred-work latency, granularity trade-offs — HIGH confidence
- [SEI CERT C++ CON52-CPP](https://wiki.sei.cmu.edu/confluence/display/cplusplus/CON52-CPP.+Prevent+data+races+when+accessing+bit-fields+from+multiple+threads): data race risk when adjacent bit-fields share storage units — HIGH confidence (official standard)
- [cppreference std::bitset](https://en.cppreference.com/w/cpp/utility/bitset.html): std::bitset is not thread-safe; no atomic operations — HIGH confidence (official)
- v1.2 Retrospective (btop++ project): shadow-atomic desync, dual-representation consistency debt, single-writer invariant as fix — HIGH confidence (first-party)
- v1.0 Retrospective (btop++ project): differential rendering (diff_and_emit vs full_emit) correctness requirements — HIGH confidence (first-party)

---
*Pitfalls research for: btop++ v1.6 — Unified Dirty-Flag Consolidation*
*Researched: 2026-03-03*
