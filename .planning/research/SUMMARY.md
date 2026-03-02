# Project Research Summary

**Project:** btop++ v1.3 — Menu PDA + Input FSM
**Domain:** C++ terminal system monitor — explicit state machine architecture extension
**Researched:** 2026-03-02
**Confidence:** HIGH

## Executive Summary

btop++ v1.3 is a pure refactoring milestone that replaces two implicit, flag-driven systems with typed, explicit state machines. The menu system currently uses a `bitset<8> menuMask` plus `int currentMenu` with function-static locals scattered across 8 menu functions — a pattern that allows structurally invalid states and makes per-frame state invisible and untestable. The input routing system uses two independent boolean flags (`Menu::active` and `Config::proc_filtering`) that must be checked in the right order to determine input mode. The migration targets are well-defined: a `std::stack<MenuFrame, std::vector<MenuFrame>>` pushdown automaton with a typed `MenuFrame = std::variant<...>` for all 8 menus, and an `InputStateVar = std::variant<Normal, Filtering, MenuActive>` FSM replacing the boolean pair. The entire existing v1.1/v1.2 infrastructure (`AppStateVar`, `AppEvent`, `dispatch_event`, `on_event`, GoogleTest v1.17.0) is reused without change.

The recommended approach follows the existing codebase pattern exactly: `std::variant` for frame types, `std::visit` for dispatch, `on_event` overloads for transitions, and no new dependencies. The architecture research confirms a clear 6-phase build order (frame types → static-local migration → bitset replacement → input FSM → tests → cleanup), driven by hard dependency constraints. The two machines couple at exactly one point: when the PDA stack empties, the Input FSM transitions back to `Normal`. The user sees zero behavior change; only internal representation changes.

The critical risk is not architectural difficulty but mechanical correctness during the incremental migration. Seven pitfalls were identified, all with well-understood prevention strategies rooted in three root causes: dual state representations desyncing during the transition window, the C++ variant self-modification rule (visitor holds a reference, stack is modified during visit causing dangling reference UB), and the resize path (frame structs storing stale absolute coordinates). All seven pitfalls are preventable by decisions made before coding starts: the `PDAAction` return-value pattern, the single-writer wrapper for `menuMask`/PDA during migration, the layout/interaction struct split, and per-frame mouse mapping ownership.

---

## Key Findings

### Recommended Stack

The v1.3 implementation requires zero new dependencies. All technology is already present in the codebase or the C++ standard library: `std::stack<MenuFrame, std::vector<MenuFrame>>` (C++11, `<stack>`), `std::variant` and `std::visit` (C++17, already used in `btop_state.hpp` and `btop_events.hpp`), the overload pattern (C++20, CTAD for aggregates, already used), and GoogleTest v1.17.0 via FetchContent (already in `CMakeLists.txt`). The vector backing store for the PDA stack is explicitly preferred over the default deque: menu depth is bounded at 3–4 levels, `std::vector<MenuFrame>` with `reserve(8)` uses a single heap allocation.

See `.planning/research/STACK.md` for full rationale and code patterns.

**Core technologies:**
- `std::stack<MenuFrame, std::vector<MenuFrame>>`: PDA container — contiguous memory, bounded-depth, zero extra allocations over a single reserve call
- `std::variant<menu_frame::Main, menu_frame::Options, ...>` (`MenuFrame`): typed frame union — compile-time exhaustiveness, value semantics, no virtual dispatch; identical pattern to existing `AppStateVar`
- `std::variant<input_state::Normal, input_state::Filtering, input_state::MenuActive>` (`InputStateVar`): input FSM state — single typed value replaces two boolean flags; `Filtering` carries `old_filter` as a struct member
- `std::visit` two-variant dispatch: state × event transition — identical to existing `dispatch_event`, already proven at 266 tests
- Overload pattern (`template<class... Ts> struct overload : Ts...`): inline visitor construction — C++20, no explicit deduction guide needed
- GoogleTest `TEST()` / `TEST_P` / `INSTANTIATE_TEST_SUITE_P`: unit tests for PDA and Input FSM — already wired in CMakeLists.txt

### Expected Features

The milestone delivers zero user-visible features. The MVP is behavior parity with internal representation changed. Full detail in `.planning/research/FEATURES.md`.

**Must have (table stakes — all P1):**
- `MenuFrame` variant defined for all 8 menus with per-frame state as struct members (replacing function-static locals)
- `push(frame)`, `pop()`, `replace(frame)` PDA operations replacing `menuMask.set/reset` and `currentMenu`
- Input FSM: `Normal`, `Filtering` (carries `old_filter`), and `MenuActive` states
- All Input FSM transitions wired: `Normal↔Filtering`, `Normal↔MenuActive` driven by PDA empty/non-empty
- Mouse routing via `InputStateVar` check instead of `Menu::active` bool
- PDA transition tests (`test_menu_pda.cpp`) and Input FSM transition tests (`test_input_fsm.cpp`)
- ASan + UBSan + TSan clean; all 266 existing tests passing

**Should have (architectural differentiators — fall out of the design, no extra work):**
- Illegal menu combinations structurally unrepresentable (variant stack enforces one active frame per slot)
- Per-frame state lifetime matches frame lifetime (RAII destroys state on pop; no stale static locals)
- Compiler-enforced frame handling (adding a new frame type without handling it in dispatch is a compile error)
- `input_state::Filtering` carries `old_filter` in state struct, eliminating one file-scoped global variable

**Defer (v1.4+):**
- Transition logging infrastructure
- Multi-level push beyond 3–4 levels (architecture supports it; not needed for parity)

**Anti-features (do not build):**
- Deep push of Main on stack below Options/Help — ESC from Options must return to Normal, not Main; this is a regression
- Async or coroutine-based menu state machines — would introduce races with Runner's `pause_output` and `bg` string
- External FSM libraries (Boost.SML, TinyFSM) — violates the no-new-dependencies constraint in PROJECT.md

### Architecture Approach

The target architecture inserts two new components — `MenuPDA` and `InputStateVar` — between the existing main loop input poll and the existing menu rendering functions, replacing three lines of `if (Menu::active) ... else ...` logic with typed dispatch. The `AppStateVar` / Runner FSM / event queue remain completely unchanged. The main loop's input routing becomes `Input::FSM::process(key, input_var, menu_pda)`, which internally routes to the PDA when in `MenuActive` state. The two machines couple at one point only: PDA empty → Input FSM transitions to `Normal`.

See `.planning/research/ARCHITECTURE.md` for full system diagram, data flow sequences, and component boundary table.

**Major components:**
1. `MenuPDA` (`btop_menu_pda.hpp`) — owns `std::vector<MenuFrameVar>` stack; provides `push`, `pop`, `replace`, `top`, `empty`, `process(key) -> PDAAction`; main-thread only; dispatches to top frame via `std::visit`
2. `InputStateVar` (`btop_input_fsm.hpp`) — owns current input mode (`Normal` / `Filtering` / `MenuActive`); replaces `Menu::active` bool and `Config::proc_filtering` routing flag; transitions drive PDA push/pop and vice versa
3. Modified `Menu::` namespace — `active` atomic bool and `menuMask` removed; `currentMenu` removed; rendering functions receive frame data by reference instead of reading statics; `Menu::mouse_mappings` retained, populated by active frame's render call
4. Modified `Input::process()` — dispatches via `InputStateVar` instead of `if (filtering)` branch; `old_filter` file-scoped variable moved into `input_state::Filtering` struct
5. Modified `btop.cpp` main loop — single `InputFSM::process(key, input_var, pda)` call replaces the `if/else` at line 1572–1574

**Build order (dependency-driven, from ARCHITECTURE.md Phase A–F):**
- Phase A: `MenuFrameVar` types + PDA skeleton (data structures only, no UI changes)
- Phase B: Migrate function-statics into frame structs (behavior-preserving rendering change)
- Phase C: Replace `menuMask`/`currentMenu`/`menuFunc` with PDA dispatch
- Phase D: `InputStateVar` FSM (depends on C — `MenuActive` must be stable before FSM delegates to PDA)
- Phase E: Tests (depends on D — exercises full integration)
- Phase F: Cleanup — remove `Menu::active`, `old_filter` global, dead declarations

### Critical Pitfalls

Top pitfalls with prevention strategies. Full detail and recovery plans in `.planning/research/PITFALLS.md`.

1. **Variant self-modification UB during `std::visit`** — Frame handlers must return a `PDAAction` enum (Push/Pop/Replace/NoChange); the caller applies the action after the visitor completes. Never mutate the PDA stack from inside a `std::visit` call on that same stack's top frame. Establish the `PDAAction` return-value pattern before any frame is implemented (Phase A). Run ASan + rapid menu open/close/open test sequences.

2. **Static local state not reset on re-entry** — Audit every `static` local in all 8 menu functions before writing any frame struct. Frame struct constructors must unconditionally initialize every field to its fresh-open value; delete the `bg.empty()` reset sentinel pattern. Write a reopen-state test per menu. The `frame::Options` struct has 15 state members and is the highest-risk target.

3. **Dual state desync during incremental migration** — Wrap all `menuMask.set/reset` and `currentMenu` assignments in a single function (`menu_show()`/`menu_close()`) that updates both representations atomically, introduced before the first menu is migrated. Add `assert(menuMask.none() == pda_stack.empty())` to `Menu::process()` during the transition. Remove `menuMask` completely in the phase that converts the last menu — do not run both in parallel across multiple phases.

4. **Mouse mapping ownership ambiguity** — Each PDA frame struct owns its mouse mappings as a value member. `Input::get()` reads a stable pointer set by `Menu::process()` after each PDA transition, not during input parsing. No merging of mappings across frames; only the topmost frame's mappings are ever active. Settle this ownership model in Phase A before any Input FSM state struct is implemented.

5. **`proc_filtering` dual source of truth** — `InputStateVar` must be the authoritative source for filtering mode. All code paths that previously called `Config::set(BoolKey::proc_filtering)` must be replaced with FSM transitions in the same phase the FSM is introduced (Phase D). Grep for every read of `proc_filtering` and verify each is replaced. Do not defer this.

6. **Stale coordinates on resize** — Split each frame struct into `layout` fields (`x, y, width, height, bg`) and `interaction` fields (`selected, page, entered text`). On resize, call `invalidate_layout()` to zero layout fields while preserving interaction. Establish this split in the initial frame struct definition (Phase A). The existing `on_enter(Resizing)` App FSM path that calls `Menu::process()` must also call `menu_pda.invalidate_layout()`.

7. **Re-entrant push during `std::visit`** — The existing `Switch` return code in `menuFunc` causes `mainMenu()` to call `menuMask.set(Menus::Options)` directly, then return `Changed`, which triggers a recursive `Menu::process()` call — effectively pushing while inside the visitor. With a PDA this is UB (see pitfall 1). Frame handlers must return `PDAAction::Replace(frame)` not call `Menu::show()` directly. `Menu::show()` must only be called from outside `Menu::process()`.

---

## Implications for Roadmap

Based on combined research, the architecture research already defines the correct build order. The roadmap should follow the six-phase dependency chain exactly. Each phase is a meaningful, independently validatable unit of work.

### Phase 1: Frame Data Structures + PDA Skeleton

**Rationale:** Everything depends on frame types existing. This phase produces no behavior change, making it safe to validate in isolation. The `PDAAction` return-value pattern (preventing pitfall 1 and 7) and the layout/interaction struct split (preventing pitfall 6) must be established here — retrofitting them into implemented frames is expensive and error-prone.
**Delivers:** `MenuFrameVar` variant with all 8 frame structs (layout + interaction fields separated), `MenuPDA` class with `push/pop/replace/top/empty` and `process(key) -> PDAAction`, `PDAAction` enum definition, `btop_menu_pda.hpp` header. Per-frame `mouse_mappings` ownership model established.
**Addresses:** MenuFrame variant + frame structs (P1 feature), PDA core operations scaffold, mouse mapping ownership (P1)
**Avoids:** Variant self-modification UB (PDAAction pattern established upfront), stale coordinates on resize (layout/interaction split), mouse mapping ambiguity (per-frame ownership model)

### Phase 2: Static Local Migration

**Rationale:** Before switching the dispatch mechanism, the rendering code must consume frame data. This phase keeps `menuMask`/`currentMenu` alive as the dispatch authority while converting each menu function to read from its frame struct instead of function-statics. The single-writer wrapper must be introduced here — it is the migration safety net and must exist before any menu is converted.
**Delivers:** All 8 menu functions refactored to receive their frame by reference; all `static` locals removed and replaced by struct members; single-writer wrapper `menu_show()`/`menu_close()` updating both `menuMask` and PDA stack atomically; `assert(menuMask.none() == pda_stack.empty())` guard in `Menu::process()` for the transition period.
**Addresses:** Per-frame state in struct members (P1 feature)
**Avoids:** Static local state not reset on re-entry (static audit + unconditional constructor initialization), dual state desync (single-writer wrapper prevents write-one-miss-the-other)

### Phase 3: PDA Dispatch Replaces menuMask

**Rationale:** With rendering consuming frame data (Phase 2 complete), the dispatch mechanism can be switched. `menuMask`/`currentMenu`/`menuFunc` are removed; `std::visit` on `PDA::top()` becomes the sole dispatch authority. This is the highest regression risk phase — it changes the dispatch path for all 8 menus simultaneously. The `Switch` return-code re-entrant call pattern is eliminated here.
**Delivers:** `menuMask` removed; `currentMenu` removed; `menuFunc` dispatch vector removed; `Menu::show()` replaced by `MenuPDA::push(frame)`; `Switch` return code replaced by `PDAAction::Replace(frame)`; `Menu::process()` rebuilt around PDA `std::visit` dispatch; `bg` string lifecycle correctly tied to stack empty/non-empty.
**Addresses:** push/pop/replace operations wired (P1), Switch→Replace semantics (P1), background string lifecycle
**Avoids:** Re-entrant push during std::visit (PDAAction return-value enforced; no direct push from inside frame handler), dual state desync (menuMask fully removed — transition window closed)

### Phase 4: Input FSM

**Rationale:** The Input FSM's `MenuActive` state delegates to the PDA; the PDA must be the sole dispatch authority (Phase 3 complete) before this delegation is valid. The `proc_filtering` dual-truth pitfall must be resolved in this phase — not deferred to a later cleanup phase, because deferring leaves two sources of truth active across the test suite.
**Delivers:** `InputStateVar` variant defined; `on_input` overloads for all three states; `Input::process()` dispatches via FSM instead of `if (filtering)` branch; `old_filter` moved into `input_state::Filtering`; `Config::proc_filtering` removed as routing signal (retained as Config persistence value, set by FSM on transition); `Menu::active` atomic bool removed; mouse routing in `Input::get()` reads `InputStateVar`; main loop `if/else` (line 1572) replaced by `InputFSM::process(key, input_var, pda)`; `on_enter(Resizing)` updated to call `menu_pda.invalidate_layout()`.
**Addresses:** Input FSM states + all transitions (P1), mouse routing via FSM (P1), App FSM resize integration
**Avoids:** proc_filtering dual source of truth (resolved in same phase), mouse mapping ownership (wired to per-frame model from Phase 1)

### Phase 5: Tests

**Rationale:** The full integration is stable after Phase 4. Tests validate the typed interfaces that are now the system's public contract. Following the v1.1/v1.2 pattern: test state machines in isolation without starting the terminal or runner thread.
**Delivers:** `tests/test_menu_pda.cpp` — push/pop/replace invariants, frame isolation (Options data does not bleed to Help frame), SizeError override, SignalSend→SignalReturn sequence, resize invalidates layout but preserves interaction, reopen shows fresh state; `tests/test_input_fsm.cpp` — all 6 state transitions, key routing per state, mouse routing, filter entry/exit including mouse_click exit path, `pause_output` cleared on all pop paths; ASan + UBSan + TSan clean builds; all 266 existing tests passing.
**Addresses:** PDA transition tests (P1), Input FSM transition tests (P1)
**Avoids:** "Looks done but isn't" checklist items: fresh-open invariant per menu, all Filtering exit paths (keyboard + mouse), resize-with-menu-open, `pause_output` cleared on all PDA empty paths, Config save on Options close preserved

### Phase 6: Cleanup

**Rationale:** After tests confirm correctness, the migration scaffolding and aliased symbols are removed. This phase carries no behavior risk — it only deletes dead code and adds defensive guards.
**Delivers:** `Menu::active` atomic bool declaration removed from `btop_menu.hpp`; `old_filter` file-scope variable removed from `btop_input.cpp`; transition-period `assert(menuMask.none() == pda_stack.empty())` removed; stack depth defensive guard added (`assert(stack.size() <= 4)`) (P2); transition logging added to push/pop/replace if desired (P2).
**Addresses:** Remove `Menu::active` bool (P2), stack depth guard (P2)

### Phase Ordering Rationale

- Types before rendering: Frame structs must exist before rendering functions can consume them (Phase 1 → 2)
- Rendering before dispatch: Render functions must consume frame data before the dispatch mechanism switches (Phase 2 → 3); switching dispatch while renderers still read statics would cause a use-after-free
- Dispatch before Input FSM: The Input FSM's `MenuActive` branch delegates to PDA; delegation requires PDA to be the sole dispatch authority (Phase 3 → 4)
- Integration before tests: Tests exercise the full Input FSM + PDA integration; both must be complete (Phase 4 → 5)
- Tests before cleanup: Dead-code removal is safe only after correctness is confirmed (Phase 5 → 6)
- The single-writer wrapper in Phase 2 is the migration safety net: it collapses the dual-state desync window to zero by ensuring every `menuMask` write and every PDA push/pop happen together in a single function

### Research Flags

Phases with standard patterns (skip `/gsd:research-phase` — all required patterns already proven in this codebase at HIGH confidence):
- **Phase 1:** `std::variant`, `std::visit`, `std::stack<T, vector<T>>` — direct extension of existing `AppStateVar` pattern; all patterns verified via cppreference and codebase analysis
- **Phase 2:** Function-static to struct-member migration — mechanical refactor with clear audit protocol; well-understood C++
- **Phase 3:** PDA dispatch via `std::visit` — mirrors existing `dispatch_event` exactly; already proven at 266 tests
- **Phase 4:** Input FSM — direct extension of App FSM pattern already in `btop_state.hpp`
- **Phase 5:** GoogleTest `TEST`, `TEST_P`, `INSTANTIATE_TEST_SUITE_P` — already in use in the test suite with established patterns
- **Phase 6:** Dead-code removal — no research needed

No phases require `/gsd:research-phase`. All technology decisions are validated at HIGH confidence. The only prerequisite task before coding is the static-local audit for all 8 menu functions (a code-reading task, not a research task) — it must produce the authoritative field inventory for each frame struct before Phase 1 frame structs are written.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All technologies are `<stack>`, `<variant>`, `<type_traits>` — C++ standard library, verified via cppreference. Direct codebase precedent for every pattern (`AppStateVar`, `dispatch_event`, overload pattern, GoogleTest). Zero new dependencies. |
| Features | HIGH | Based on direct source code analysis of `btop_menu.cpp`, `btop_input.cpp`, `btop_menu.hpp`. MVP scope is behavior parity — no feature uncertainty. All 8 menus inventoried with per-frame state enumerated (15 fields for Options, down to 0 for SizeError). |
| Architecture | HIGH | Based on direct source code analysis with specific file+line locations identified: main loop integration at `btop.cpp:1572–1574`, mouse routing at `btop_input.cpp:182`, filtering check at `btop_input.cpp:221`, resize path at `on_enter(Resizing)` in `btop.cpp:1009`. Build order dependency graph confirmed by existing code structure. |
| Pitfalls | HIGH | Based on v1.1/v1.2 retrospectives (shadow-atomic desync is a repeated pattern, documented in `.planning/RETROSPECTIVE.md`) and direct source analysis of specific problematic code paths (`Switch` re-entrant call at `btop_menu.cpp:1866–1872`, resize path in App FSM). C++ variant self-modification rule is confirmed in cppreference and C++ Stories FSM article. |

**Overall confidence:** HIGH

### Gaps to Address

- **`replace()` semantics — model choice:** The exact modeling of Main→Options (`pop(); push(Options)` at depth 1 vs a true `replace()` that keeps stack depth at 1) must be decided in Phase 1 before any frame handler is coded. Both models satisfy the behavioral requirement (ESC from Options → Normal, not Main). Recommendation: implement `replace()` as a named operation that atomically pops and pushes in a single call — cleaner semantics than `pop(); push()` pair, avoids a transient empty-stack state that would incorrectly trigger an Input FSM `Normal` transition.

- **`Config::proc_filtering` reader audit:** The architecture research recommends retaining the Config key as a value set by the Input FSM (not the source of truth). Before Phase 4 coding, grep for every read of `proc_filtering` beyond the routing check to identify all drawing/rendering callers. These callers may be reading for display purposes (e.g., showing the filter text box) and must continue to work correctly after the FSM is introduced.

- **`Runner::pause_output` clearing on all pop paths:** The pitfalls research explicitly flags this as a "looks done but isn't" item. Phase 5 tests must verify `pause_output == false` after every path that empties the PDA stack, including error paths (SizeError close, SignalReturn close). This is not covered by unit-testable PDA invariants alone — it requires integration-level verification.

- **Static-local audit before Phase 1:** All 8 menu functions must be audited for `static` locals before any frame struct is written. The audit is a code-reading task, not research, but it is a hard prerequisite for correct frame struct field lists. The FEATURES.md already provides a partial inventory (e.g., Options has 15 fields); the audit must verify and complete this list.

---

## Sources

### Primary (HIGH confidence)
- `src/btop_menu.cpp`, `src/btop_menu.hpp` — current `menuMask`/`currentMenu`/function-static-locals implementation; all 8 menu functions and their static locals inventoried
- `src/btop_input.cpp`, `src/btop_input.hpp` — current `proc_filtering` routing at line 221, mouse dispatch at line 182
- `src/btop_state.hpp`, `src/btop_events.hpp`, `src/btop.cpp` — existing `AppStateVar` / `AppEvent` / `dispatch_event` / `on_event` / `transition_to` patterns; main loop at lines 1572–1574; resize path at line 1009
- `tests/test_app_state.cpp`, `tests/test_transitions.cpp`, `tests/test_events.cpp` — existing test patterns and GoogleTest v1.17.0 integration
- `.planning/PROJECT.md` — v1.3 scope, constraints, and no-new-dependencies rule
- `.planning/RETROSPECTIVE.md` (v1.1, v1.2) — shadow-atomic consistency debt, single-writer invariant as solution, incremental migration lessons
- [cppreference — std::stack](https://en.cppreference.com/w/cpp/container/stack) — `stack<T, vector<T>>` parameterization confirmed
- [cppreference — std::variant::visit](https://en.cppreference.com/w/cpp/utility/variant/visit) — two-variant `std::visit` form confirmed C++17; behavior when variant modified during visitation
- [GoogleTest v1.17.0 Testing Reference](https://google.github.io/googletest/reference/testing.html) — `TEST_P`, `INSTANTIATE_TEST_SUITE_P` patterns

### Secondary (MEDIUM confidence)
- [C++ Stories — FSM with std::variant (2023)](https://www.cppstories.com/2023/finite-state-machines-variant-cpp/) — two-variant dispatch, dangling reference from variant self-modification during visit
- [C++ Stories — Overload Pattern](https://www.cppstories.com/2019/02/2lines3featuresoverload.html/) — overload in C++17/20, C++23 `consteval` catch-all
- [Game Programming Patterns — State (Pushdown Automata)](https://gameprogrammingpatterns.com/state.html) — canonical push/pop/replace semantics for menu stacks
- [Implementing State Machines with std::variant](https://khuttun.github.io/2017/02/04/implementing-state-machines-with-std-variant.html) — per-state data in variant alternatives

### Tertiary (MEDIUM confidence — vendor documentation)
- [QP State Machine: State-Local Storage Pattern](https://www.state-machine.com/doc/Pattern_SLS.pdf) — per-state data ownership, layout/interaction separation (pattern is sound; source is vendor documentation)

---
*Research completed: 2026-03-02*
*Ready for roadmap: yes*
