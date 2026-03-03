# Project Research Summary

**Project:** btop++ v1.6 — Unified Dirty-Flag Mechanism
**Domain:** C++ terminal system monitor — internal rendering state consolidation
**Researched:** 2026-03-03
**Confidence:** HIGH

## Executive Summary

btop++ v1.6 is a focused architectural refactor: replacing seven scattered redraw state variables (five per-box `bool redraw` globals, a `vector<bool>` for GPU panels, a `runner_conf::force_redraw` field, and an `atomic<bool> pending_redraw`) with a single `DirtyFlags` bitset backed by `std::atomic<uint32_t>`. This is not a feature-addition milestone — the user-visible behavior must be identical before and after. The recommended approach is a `PendingDirty` struct with `fetch_or` on producers and `exchange(0)` on the runner consumer, wired in five incremental phases: type definition plus dead-code cleanup, runner integration, `calcSizes()` decoupling, per-box bool migration, and full regression validation. All patterns required are C++ standard library primitives (no new dependencies) and follow conventions already established in the codebase (shadow atomic, enum-indexed types, `[[nodiscard]]` annotations).

The key risks are not technical complexity — the patterns are well-understood — but migration discipline. There are at least 20 write sites scattered across `btop.cpp`, `btop_draw.cpp`, `btop_input.cpp`, and all platform-specific `btop_collect.cpp` files. Migrating readers before all writers, or removing per-box bool declarations before their write sites are converted, creates silent under-invalidation (boxes stop redrawing on the right triggers). The dual-write atomicity gap pitfall from v1.2's shadow-atomic desync is directly applicable: the rule is one-box-at-a-time migration where the old bool declaration is deleted in the same commit that converts its write sites to `pending_dirty.mark()`.

A second critical separation concerns the distinction between per-box content invalidation (a box needs to rebuild its static chrome) and full-screen emit (`ScreenBuffer::force_full`, which bypasses differential cell comparison). These are currently conflated via `runner_conf::force_redraw`. The `DirtyFlags` design must maintain them as separate concerns: per-box bits (Cpu, Mem, Net, Proc, Gpu0–4) drive `force_redraw` parameters into each `draw()` call; a separate `ForceFullEmit` bit drives `screen_buffer.set_force_full()`. Conflating them means every key press causes a full terminal repaint, undoing the differential rendering work from v1.0. `Menu::redraw` must also remain a standalone `bool` outside the bitset — it is consumed by the main-thread input loop, not the runner thread.

## Key Findings

### Recommended Stack

No new dependencies are required. The entire implementation uses `<atomic>`, `<cstdint>`, and `<type_traits>` from the C++ standard library, all already required by the project. The recommended storage type is `std::atomic<uint32_t>` (not `std::bitset`, which has no atomic interface). The recommended enum is `enum class DirtyBit : uint32_t` with explicit bit-shift values, following the same scoped-enum convention as `AppStateTag`, `RunnerStateTag`, and `ThemeKey` already in the codebase. Bitwise operators (`|`, `&`, `~`) are provided as `constexpr` free functions using `std::underlying_type_t<DirtyBit>` casts, following the C++20 concept-gated pattern from Andreas Fertig (2024).

See `.planning/research/STACK.md` for full rationale, code patterns, and alternatives considered.

**Core technologies:**
- `enum class DirtyBit : uint32_t`: per-box and force-emit flags — scoped enum prevents implicit int conversion; bit-shift values are self-documenting; up to 32 independent flags before widening
- `std::atomic<uint32_t>` with `fetch_or(memory_order_release)` / `exchange(0, memory_order_acq_rel)`: atomic accumulator — `fetch_or` is a single instruction on x86/ARM; `exchange(0)` atomically reads-and-clears with no missed-flag window
- `PendingDirty` struct: encapsulates the atomic, provides `mark(DirtyBit)` and `take()` — separates cross-thread dirty accumulation from the enum type definition; replaces both `pending_redraw atomic<bool>` and `runner_conf::force_redraw`
- GoogleTest + TSan: existing test infrastructure — TSan confirms no new races introduced during migration; 330 existing tests confirm no behavioral regressions

**Critical version notes:**
- `std::atomic<uint32_t>::fetch_or` requires C++11 `<atomic>` (already required by project)
- `std::to_underlying` is C++23 (optional simplification; falls back to `static_cast<underlying_type_t<E>>` without correctness loss)
- `[[nodiscard]]` on `take()` requires C++17 (already used throughout codebase)
- `std::atomic<uint32_t>` is always lock-free on all btop target platforms (x86-32, x86-64, ARMv7, ARM64)

### Expected Features

This is a refactoring milestone. All features are internal. The MVP delivers behavioral parity with improved internal structure. Full detail in `.planning/research/FEATURES.md`.

**Must have (table stakes — v1.6 is incomplete without these):**
- `DirtyBit` enum and `PendingDirty` struct defined in `btop_dirty.hpp` — foundation; all other changes depend on this
- `mark(DirtyBit)` / `take()` / `any(DirtyBit)` API covering all existing redraw operations — every current flag operation must be expressible
- Per-box namespace bools (`Cpu::redraw`, `Mem::redraw`, `Net::redraw`, `Proc::redraw`, `Gpu::redraw vector<bool>`) removed and replaced by bitset bits — core goal
- `runner_conf::force_redraw` removed; runner reads `pending_dirty.take()` once per cycle — eliminates split responsibility
- `Draw::calcSizes()` calls `pending_dirty.mark(DirtyBit::All)` instead of direct namespace bool assigns — decouples geometry computation from dirty marking
- `request_redraw()` implementation replaced: `pending_redraw.store()` → `pending_dirty.mark(DirtyBit::All)` — eliminates the two-step fold
- Dead `Proc::resized atomic<bool>` removed with explicit `calcSizes()` branch simplification — eliminates dead code and the latent read-of-always-false
- Four local `bool redraw` variables in `btop_input.cpp` renamed to `force_redraw` — eliminates naming collision
- All 330 existing tests pass; ASan/UBSan/TSan clean — no behavioral regressions

**Should have (architectural payoff — fall out of the design at no extra cost):**
- Per-box selective redraw: only dirty boxes rebuild static chrome; reduces wasted string-building in steady-state cycles
- Inspectable dirty state: a single `PendingDirty` object can be debug-printed; currently requires instrumenting 7 separate variables across 5 namespaces
- `[[nodiscard]]` on `take()` to prevent accidentally discarding the flag snapshot without acting on it

**Defer to v1.7+:**
- Per-GPU-panel dirty bits (one bit per GPU index instead of one `Gpu` bit for all panels) — current single-bit approach matches existing `force_redraw` behavior
- Removing `bool force_redraw` parameter from individual `draw()` function signatures — conservative Phase 4 preserves signatures; follow-on phase can clean them up
- Async DirtyFlags observable or multi-consumer model — btop has exactly one consumer (runner thread)

**Anti-features (do not build):**
- Sub-box dirty regions (line-level or cell-level) — btop's `ScreenBuffer` already handles cell-level differential rendering; DirtyFlags operates at box level only
- `Menu::redraw` absorbed into `DirtyBit` — menu rendering is main-thread only and consumed synchronously; adding it to an atomic accumulator adds complexity without benefit
- `std::atomic<std::bitset<N>>` as the underlying type — `std::bitset` is not trivially copyable in all implementations; use `std::atomic<uint32_t>` instead

### Architecture Approach

The architecture introduces one new file (`src/btop_dirty.hpp`) with the `DirtyBit` enum and `PendingDirty` struct, plus a `pending_dirty` instance in the `Runner` namespace in `btop.cpp`. All existing producers (main thread via `Runner::run()`, `Draw::calcSizes()`, cross-thread via `request_redraw()`, platform-specific collect functions) call `pending_dirty.mark(bits)`. The runner thread calls `pending_dirty.take()` once per cycle at the start of the draw phase, getting an atomic snapshot of all accumulated dirty bits, then derives per-box `bool force_redraw` values from the snapshot before calling each `draw()` function. This conservative approach preserves all `Xxx::draw(data, force_redraw, no_update)` signatures unchanged, minimizing the refactor surface. `ScreenBuffer::force_full` and `Menu::redraw` remain separate — they are not cross-thread dirty flags.

See `.planning/research/ARCHITECTURE.md` for full system diagrams, data flow sequences, thread-safety analysis, and component boundary table.

**Major components:**
1. `DirtyBit` enum + `PendingDirty` struct (`src/btop_dirty.hpp`) — bitmask type and atomic accumulator; zero dependencies; fully unit-testable in isolation; encapsulates `fetch_or`/`exchange` atomics behind a clean API
2. `Runner::_runner()` draw phase (`src/btop.cpp`) — sole consumer; calls `pending_dirty.take()` once per cycle, derives per-box booleans, passes them to unchanged `draw()` function signatures
3. `Draw::calcSizes()` (`src/btop_draw.cpp`) — primary bulk producer; replaces the 4-namespace bool mass-assign with `pending_dirty.mark(DirtyBit::All)`, decoupling geometry computation from redraw forcing
4. `Runner::run()` + `request_redraw()` (`src/btop.cpp`) — main-thread producers; replace `pending_redraw atomic<bool>` + `force_redraw` struct field with `pending_dirty.mark()`
5. Platform collect functions (`linux/`, `osx/`, `freebsd/`, `netbsd/`, `openbsd/` `btop_collect.cpp`) — secondary producers; scattered `Net::redraw = true` / `Mem::redraw = true` write sites must each become `pending_dirty.mark(DirtyBit::Net)` etc.

**Build/phase order (from ARCHITECTURE.md phases A–E):**
- Phase A (type + cleanup) must precede all integration phases
- Phase C (runner integration) must precede Phase D (calcSizes decoupling) — `pending_dirty` must exist before calcSizes can call it
- Phase D must precede Phase E (per-box bool removal) — per-box bools remain as short-circuits inside draw functions until Phase E
- Phase B (dead code + rename) is independent and can be merged with Phase A

### Critical Pitfalls

Top pitfalls with prevention strategies. Full detail, warning signs, and recovery plans in `.planning/research/PITFALLS.md`.

1. **Dual-write atomicity gap during migration** — Reader migrated to `DirtyFlags` while a write site still uses the old per-box bool; box silently stops redrawing on that trigger. Prevention: delete the old `bool redraw` declaration in the same commit that migrates its write sites. Compile errors from remaining write sites become the migration checklist. Never leave a partial migration where the write path is old-style but the read path is `DirtyFlags`.

2. **Missing write sites produce under-invalidation** — At least 20 write sites exist across 6+ files including self-invalidations inside `draw()` functions (e.g., `Net::draw()` sets `redraw = true` when IP address changes). Resize-triggered redraws mask missing steady-state invalidations. Prevention: enumerate all write sites with `grep -rn "redraw\s*=" src/` before writing any code; classify as "self-invalidation inside draw" vs "external trigger"; migrate each explicitly.

3. **Per-box bits and ForceFullEmit conflated — differential rendering broken** — If the per-box dirty path also triggers `screen_buffer.set_force_full()`, every key press causes a full terminal repaint instead of differential emit. Prevention: `ForceFullEmit` must be a separate, distinct bit set only for resize/reload events; per-box content dirty bits must never trigger `set_force_full()`. Verify after migration that single-key-press cycles use `diff_and_emit` not `full_emit`.

4. **Cross-thread race on plain bitset** — `std::bitset<N>` has no atomic interface; using it as the cross-thread dirty accumulator is a data race. Prevention: `PendingDirty` struct encapsulates `std::atomic<uint32_t>` with correct memory ordering; TSan on the migrated build confirms correctness.

5. **`calcSizes()` decoupling without updating all call sites** — Removing redraw-forcing from `calcSizes()` before all 5 call sites are updated to mark dirty produces frames where boxes render at new geometry coordinates with stale content strings. Prevention: dedicated decoupling phase with all 5 call sites enumerated and updated atomically; add `assert(pending_dirty.any())` in debug builds after `calcSizes()` returns.

## Implications for Roadmap

Based on combined research, 5 phases are indicated by the architectural dependency chain from ARCHITECTURE.md. The build order A → C → D → E (with B independent) maps directly to roadmap phases.

### Phase 1: DirtyFlags Foundation — Type Definition, Dead Code Removal, Naming Cleanup

**Rationale:** The `DirtyBit` enum and `PendingDirty` struct must exist before any integration site can use them. The rename of local `bool redraw` in `btop_input.cpp` and removal of `Proc::resized` dead code are independent but best done first to reduce diff noise in later phases and to establish the naming contract before any flag logic is touched. Both cleanups have zero behavioral risk.
**Delivers:** `src/btop_dirty.hpp` with `DirtyBit` enum, `PendingDirty` struct, bitwise helpers; `Proc::resized atomic<bool>` removed with explicit `calcSizes()` branch simplification (condition `not (Proc::resized or ...)` → `app_state != Resizing`); four `btop_input.cpp` local variables renamed from `bool redraw` to `bool force_redraw`; unit tests for bitset operations and `take()` atomicity; compile clean on all platforms
**Addresses:** DirtyFlags type (P1), dead code removal (P1), naming collision fix (P1)
**Avoids:** Pitfall 4 (cross-thread race — `PendingDirty` encapsulates atomic from the start); Pitfall 8 (naming collision — first commit establishes clean names before any migration)

### Phase 2: Runner Integration — Replace `pending_redraw` and `runner_conf::force_redraw`

**Rationale:** The runner is the sole consumer of dirty state. Wiring `pending_dirty` into `Runner::run()` and `_runner()` is the central integration that makes the new type useful. `request_redraw()` implementation changes in this phase. The `ForceFullEmit` / per-box granularity separation must be established correctly here — correcting it later requires touching the runner again.
**Delivers:** `runner_conf::force_redraw` field removed; `pending_redraw atomic<bool>` removed; `Runner::run()` calls `pending_dirty.mark()` for force-redraw requests; `_runner()` calls `pending_dirty.take()` once per cycle and derives per-box booleans before draw calls; `request_redraw()` implementation changed from `pending_redraw.store()` to `pending_dirty.mark(DirtyBit::All)`; `ForceFullEmit` bit wired to `screen_buffer.set_force_full()` exclusively; all 330 tests pass; TSan clean
**Uses:** `PendingDirty` from Phase 1; `memory_order_release` on producers, `memory_order_acq_rel` on `take()`
**Implements:** Runner-thread consumer + main-thread producer wiring (ARCHITECTURE.md Integration Points 1, 2, 4)
**Avoids:** Pitfall 1 (old `pending_redraw` deleted in same commit); Pitfall 3 (`ForceFullEmit` is separate from per-box bits; `set_force_full()` never triggered by per-box dirty paths)

### Phase 3: `calcSizes()` Decoupling — Remove Per-Namespace Bool Assigns

**Rationale:** `calcSizes()` is the primary bulk producer of dirty state, setting all box redraws on every resize. After Phase 2, `pending_dirty` exists and the runner consumes it. `calcSizes()` can now call `pending_dirty.mark(DirtyBit::All)` and drop the direct namespace bool writes. This is the primary architectural decoupling the milestone exists to achieve — a geometry function should not reach into five different namespaces to set rendering flags.
**Delivers:** `calcSizes()` body replaces `Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true` and `Runner::request_redraw()` with a single `pending_dirty.mark(DirtyBit::All)`; GPU panel resize path (`Gpu::redraw.resize()` + `redraw[i] = true`) replaced by `DirtyBit::Gpu0..Gpu4` marks; all 5 `calcSizes()` call sites audited and confirmed to mark dirty after decoupling; resize and reload paths tested explicitly
**Avoids:** Pitfall 5 (all 5 call sites updated atomically; debug assert after calcSizes confirms dirty bits are set)

### Phase 4: Per-Box Bool Migration — Replace Namespace Redraw Bools

**Rationale:** With `pending_dirty` wired into the runner (Phase 2) and `calcSizes()` using it (Phase 3), the per-box namespace bools can now be removed. This is the highest-coupling phase because draw functions both read and write these bools internally. Each box is migrated one at a time: write sites converted to `pending_dirty.mark()`, declaration deleted, compile confirms no remaining write sites.
**Delivers:** All 5 per-box namespace redraw bools removed (`Cpu::redraw`, `Mem::redraw`, `Net::redraw`, `Proc::redraw`, `Gpu::redraw vector<bool>`); all write sites across platform-specific collect files migrated (Linux, macOS, FreeBSD, NetBSD, OpenBSD); self-invalidations inside `draw()` functions preserved as `pending_dirty.mark()` calls (e.g., `Net::draw()` IP-change trigger); `grep -rn "redraw\s*=\s*true" src/` returns zero namespace-bool hits after migration
**Avoids:** Pitfall 1 (deletion-on-migrate rule: old bool deleted in same commit as write sites are converted); Pitfall 2 (write-site inventory from pre-phase audit covers all platform collect files including self-invalidation inside draw functions)

### Phase 5: Validation and Full Regression

**Rationale:** After all migration phases, a dedicated validation phase confirms the full system is correct end-to-end before the milestone is declared complete. The "Looks Done But Isn't" checklist from PITFALLS.md has 8 items that each require deliberate verification — not just passing CI.
**Delivers:** Full test suite (330 tests) passes; ASan/UBSan/TSan clean builds confirmed; manual verification of all redraw triggers (resize, IP change, disk mount change, network interface change, sort order toggle, filter change, menu open/close); performance check confirming single-key-press cycles use `diff_and_emit` not `full_emit`; GPU panel redraw verified for all 5 panel indices; `Menu::redraw` standalone bool preserved and correctly set on menu-active resize

### Phase Ordering Rationale

- Phase 1 before all others: the `PendingDirty` type and `DirtyBit` enum must exist before any integration site can use them; cleanup commits reduce noise in later diffs
- Phase 2 before Phases 3–4: `pending_dirty` must be wired into the runner (consumer exists) before producers are migrated away from the old flag mechanism
- Phase 3 before Phase 4: `calcSizes()` is the highest-volume producer; decoupling it while per-box bools still exist is lower risk than simultaneously decoupling `calcSizes()` and removing the bools
- Phase 4 last for per-box bools: these bools are both read and written inside draw functions; removing them requires changing both namespace declarations and draw function internals; highest coupling, best done after all other infrastructure is in place
- Phase 5 is explicit: validation is not implicit in "tests pass"; the 8-item checklist requires deliberate manual verification steps that CI does not cover

### Research Flags

Phases with standard patterns (skip `/gsd:research-phase`):
- **Phase 1:** `enum class` with bitwise operators and `std::atomic<uint32_t>` with `fetch_or`/`exchange` are fully documented C++ standard patterns; cppreference + Fertig (2024) confirm implementation details at HIGH confidence; no unknowns
- **Phase 2:** Runner integration follows the existing Shadow Atomic pattern already used for `runner_state_tag`; no new patterns needed
- **Phase 5:** Validation procedures are standard; existing CI configs for ASan/TSan already exist in `build-asan/`, `build-tsan/` directories

Phases likely needing targeted research (code audit, not external research) during planning:
- **Phase 3:** Enumerate all 5 `calcSizes()` call sites and trace each call chain end-to-end; the `btop_menu.cpp:screen_redraw` call site (menu-driven layout change after options close) is the most likely to have a gap in the dirty-marking chain
- **Phase 4:** Enumerate all per-box redraw write sites across all 5 platform-specific `btop_collect.cpp` files; the write-site inventory is the prerequisite for Phase 4 and must be completed before coding begins; FEATURES.md confirms at least 15 distinct write sites exist

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All technologies are C++ standard library primitives; cppreference + Fertig (2024) confirm every pattern; no third-party libraries; no version uncertainty given GCC 12+/Clang 15+ baseline already required |
| Features | HIGH | Deep codebase analysis of all 7 existing redraw variables; all write sites inventoried in FEATURES.md; MVP definition is a strict behavior-preserving refactor with clear completion criteria; granularity analysis confirms box-level is the correct abstraction level |
| Architecture | HIGH | All integration points verified by direct source code tracing (`btop.cpp` lines 424–955, `btop_draw.cpp` calcSizes and all box draw functions, `btop_input.cpp`); data flow diagrams in ARCHITECTURE.md are grounded in actual code paths; thread-safety analysis identifies the single cross-thread object (`PendingDirty`) and its correct memory ordering |
| Pitfalls | HIGH | Pitfalls derived from first-party project retrospectives (v1.0, v1.2), direct codebase analysis, and canonical references (Game Programming Patterns dirty flag chapter, SEI CERT CON52-CPP); 8 concrete pitfalls with warning signs and recovery strategies |

**Overall confidence:** HIGH

### Gaps to Address

- **Platform collect file write-site inventory:** The write-site enumeration covers `btop_draw.cpp` in detail and notes that macOS/FreeBSD/NetBSD/OpenBSD `btop_collect.cpp` files contain `Net::redraw = true` / `Mem::redraw = true` sites. A complete per-platform list has not been produced. This must be created as the first deliverable of Phase 4 planning. Risk is medium: the patterns are consistent across platforms, but missing a single write site produces a silent under-invalidation.

- **GPU panel count range:** ARCHITECTURE.md states the maximum GPU panel count is 5 (indices 0–4), inferred from `Gpu::shown_panels`. If the actual maximum exceeds 5, the `DirtyBit::Gpu0..Gpu4` design needs widening. Validate during Phase 1 or 3 planning by reading GPU initialization code.

- **`btop_menu.cpp:screen_redraw` call path:** There are 5 `calcSizes()` call sites; the menu-driven one at `btop_menu.cpp:1647` is the most likely to lack a corresponding dirty-marking step after Phase 3 decoupling. This call site must be traced end-to-end during Phase 3 planning to confirm the menu→calcSizes→mark_all→runner path is complete.

## Sources

### Primary (HIGH confidence)
- `src/btop.cpp` (Runner namespace, lines 424–955) — all runner-thread redraw paths, `pending_redraw`, `runner_conf::force_redraw`, `request_redraw()`
- `src/btop_draw.cpp` (calcSizes, all box draw functions, lines 1039–2939) — all per-box redraw bool write/read sites, calcSizes decoupling opportunity
- `src/btop_shared.hpp`, `src/btop_input.cpp`, `src/btop_state.hpp` — shared declarations, input handler local redraw variables, dead `Proc::resized`
- `linux/btop_collect.cpp`, `osx/btop_collect.cpp`, `freebsd/btop_collect.cpp`, `netbsd/btop_collect.cpp`, `openbsd/btop_collect.cpp` — per-platform write sites for `Net::redraw` and `Mem::redraw`
- [cppreference — `std::atomic<T>::fetch_or`](https://en.cppreference.com/w/cpp/atomic/atomic/fetch_or.html) — confirmed `fetch_or` is a member of `atomic<Integral>`; memory_order parameter documented
- [cppreference — `std::memory_order`](https://en.cppreference.com/w/cpp/atomic/memory_order.html) — release/acquire producer-consumer pattern
- [cppreference — `std::bitset`](https://en.cppreference.com/w/cpp/utility/bitset.html) — confirmed no atomic interface; requires external synchronization
- btop++ v1.2 Retrospective (first-party) — shadow-atomic desync, dual-representation consistency debt, single-writer invariant as fix
- btop++ v1.0 Retrospective (first-party) — differential rendering (`diff_and_emit` vs `full_emit`) correctness requirements

### Secondary (MEDIUM confidence)
- [Andreas Fertig — C++20 Concepts applied: Safe bitmasks using scoped enums (2024)](https://andreasfertig.com/blog/2024/01/cpp20-concepts-applied/) — concept-gated bitwise operator overloads on scoped enums; confirmed pattern
- [preshing.com — You Can Do Any Kind of Atomic Read-Modify-Write Operation](https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/) — `fetch_or` as specialized single-instruction RMW; `exchange(0)` as consume-and-clear primitive
- [Game Programming Patterns — Dirty Flag](https://gameprogrammingpatterns.com/dirty-flag.html) — canonical pitfalls: granularity spectrum, deferred-work latency, propagation pitfalls, narrow mutation API
- [SEI CERT C++ CON52-CPP](https://wiki.sei.cmu.edu/confluence/display/cplusplus/CON52-CPP.+Prevent+data+races+when+accessing+bit-fields+from+multiple+threads) — data race risk on non-atomic bit-fields accessed from multiple threads

### Tertiary (LOW confidence)
- Community consensus on `is_always_lock_free` for `atomic<uint32_t>` on x86/ARM — lock-free on all btop target platforms; C++ standard only guarantees `atomic_flag` is always lock-free; community consensus + preshing corroborate for `uint32_t`

---
*Research completed: 2026-03-03*
*Ready for roadmap: yes*
