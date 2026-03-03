# Stack Research

**Domain:** C++ unified dirty-flag mechanism for btop++ terminal monitor (v1.6 milestone)
**Researched:** 2026-03-03
**Confidence:** HIGH

## Context

This file covers only what is **new or changed** for v1.6. The existing validated stack
(C++20/23, GCC 12+/Clang 15+, std::variant FSMs, lock-free event queue, enum-indexed
arrays, nanobench, GoogleTest) is carried forward unchanged.

The v1.6 work has one new implementation surface: a **DirtyFlags bitset** that replaces
six separate per-box `bool redraw` fields, `force_redraw`, and `pending_redraw` with a
single unified, atomically-settable flag set.

**What currently exists (to be replaced):**
- `Cpu::redraw`, `Mem::redraw`, `Net::redraw`, `Proc::redraw` — bool statics in namespace scope inside `btop_draw.cpp`
- `Gpu::redraw` — `std::vector<bool>` (one entry per GPU panel)
- `Menu::redraw` — bool in `btop_menu.hpp`
- `Runner::pending_redraw` — `static std::atomic<bool>` inside `btop.cpp`, folded into `conf.force_redraw` by `run()`
- `runner_conf::force_redraw` — bool copied into the per-run config struct passed to the runner thread
- `Proc::resized` — `atomic<bool>` declared in `btop_shared.hpp`, written in `btop_draw.cpp`, **never read** (dead code)

**Threading model that constrains the design:**
- Main thread: input, event dispatch, FSM transitions, calls `Runner::run()`
- Runner thread: collect + draw, reads `conf.force_redraw` from a struct snapshot
- `calcSizes()` runs on the main thread, sets `Cpu::redraw = Mem::redraw = ... = true` then calls `Runner::request_redraw()` which sets `pending_redraw`
- Cross-thread dirty signal today: main thread calls `request_redraw()` → sets `atomic<bool> pending_redraw` → runner folds it into `force_redraw` at the start of each cycle

---

## Recommended Stack

### Core Technologies (NEW for v1.6)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **`enum class DirtyFlag : uint8_t` with bit-shift values** | C++11, available now | Enumerate individual dirty causes | Scoped enum prevents implicit int conversion, `uint8_t` underlying type keeps the total footprint to 1 byte. Bit-shift values (`1u << 0`, `1u << 1`, ...) make the bit-position self-documenting. Up to 8 independent flags fit in `uint8_t`; if GPU panel count makes 8 insufficient, widen to `uint16_t`. **Confidence: HIGH** — C++ standard, cppreference confirmed |
| **`std::atomic<uint8_t>` (or `uint16_t`) as the storage type** | C++11, `<atomic>` | Atomic cross-thread dirty signal | `fetch_or` (set bits) and `exchange(0)` (consume-and-clear) are both single-instruction on x86/ARM for 8/16/32-bit integers. `std::atomic<uint32_t>::is_always_lock_free` is true on all btop target platforms (x86-32, x86-64, ARMv7, ARM64). `std::bitset` is explicitly **not** atomic and has no `fetch_or` — it cannot be used as the cross-thread store. **Confidence: HIGH** — cppreference `atomic/fetch_or` confirmed; `is_always_lock_free` confirmed for uint32_t on x86/ARM |
| **Free `constexpr` bit-operator overloads on `DirtyFlag`** | C++20 concepts pattern | Set/clear/test flags without raw casts | Operator `|`, `&`, `~` overloaded on the enum using `std::underlying_type_t<DirtyFlag>`. C++20 concepts allow opt-in via an `enable_bitmask_operators` tag — identical pattern verified by Andreas Fertig (2024). C++23 `std::to_underlying` eliminates the `static_cast<underlying>` call if the project targets C++23 (it does for some paths). **Confidence: HIGH** — cppreference + Fertig blog verified |
| **`fetch_or(bits, memory_order_release)` for producers** | C++11 `<atomic>` | Set dirty bits from any thread | `fetch_or` is a read-modify-write operation that atomically ORs bits in. Using `memory_order_release` ensures that all memory writes before the flag set are visible to the consumer when it acquires. This is the correct ordering for a "data is ready, read it now" notification. Using `memory_order_relaxed` would be incorrect — it provides no synchronization with the data the flag announces. **Confidence: HIGH** — cppreference memory_order documentation confirmed |
| **`exchange(0, memory_order_acquire)` for the consumer** | C++11 `<atomic>` | Consume-and-clear in the runner | `exchange(0)` atomically reads all set bits and clears them in one instruction — no separate load + store, no window for missed flags. `memory_order_acquire` synchronizes with the producers' `release` stores. The return value is the snapshot of which flags were set. **Confidence: HIGH** — cppreference atomic confirmed |

### The Recommended DirtyFlags Pattern

```cpp
// btop_dirty.hpp  (new header, included by btop_shared.hpp)

#include <atomic>
#include <cstdint>
#include <type_traits>

/// Individual dirty-flag bits.
/// Underlying type uint8_t — fits all 6 current flags with room for 2 more.
/// If GPU panels ever require >8 independent bits, widen to uint16_t here.
enum class DirtyFlag : uint8_t {
    None    = 0,
    Cpu     = 1u << 0,  ///< Cpu box needs full redraw
    Mem     = 1u << 1,  ///< Mem box needs full redraw
    Net     = 1u << 2,  ///< Net box needs full redraw
    Proc    = 1u << 3,  ///< Proc box needs full redraw
    Gpu     = 1u << 4,  ///< All GPU panels need full redraw
    Menu    = 1u << 5,  ///< Menu overlay needs full redraw
    All     = 0xFF,     ///< Redraw every box
};

/// Enable bitwise operators on DirtyFlag.
/// C++20 concept-gated opt-in from Andreas Fertig (2024).
template<typename T>
concept IsDirtyFlag = std::is_same_v<T, DirtyFlag>;

constexpr DirtyFlag operator|(DirtyFlag a, DirtyFlag b) noexcept {
    using U = std::underlying_type_t<DirtyFlag>;
    return static_cast<DirtyFlag>(static_cast<U>(a) | static_cast<U>(b));
}
constexpr DirtyFlag operator&(DirtyFlag a, DirtyFlag b) noexcept {
    using U = std::underlying_type_t<DirtyFlag>;
    return static_cast<DirtyFlag>(static_cast<U>(a) & static_cast<U>(b));
}
constexpr DirtyFlag operator~(DirtyFlag a) noexcept {
    using U = std::underlying_type_t<DirtyFlag>;
    return static_cast<DirtyFlag>(~static_cast<U>(a));
}
constexpr bool operator!(DirtyFlag a) noexcept { return a == DirtyFlag::None; }

namespace Runner {
    /// Cross-thread dirty flag register.
    /// Producer (main thread, input handlers): fetch_or with memory_order_release
    /// Consumer (runner thread): exchange(0) with memory_order_acquire
    extern std::atomic<uint8_t> dirty_flags;

    /// Mark one or more boxes as needing a full redraw on the next cycle.
    /// Safe to call from any thread (main, signal handler via event queue).
    inline void mark_dirty(DirtyFlag flags) noexcept {
        dirty_flags.fetch_or(
            static_cast<uint8_t>(flags),
            std::memory_order_release
        );
    }

    /// Consume and clear all dirty flags.
    /// Called once per runner cycle at the start of the draw pass.
    /// Returns the snapshot of which flags were set.
    [[nodiscard]] inline DirtyFlag consume_dirty() noexcept {
        return static_cast<DirtyFlag>(
            dirty_flags.exchange(0, std::memory_order_acquire)
        );
    }

    /// Test whether a specific flag is set without clearing.
    /// Use only from the consumer thread after consume_dirty() for single-cycle decisions.
    constexpr bool is_dirty(DirtyFlag snapshot, DirtyFlag bit) noexcept {
        return !!(snapshot & bit);
    }
}
```

**Integration at the runner call site:**

```cpp
// btop.cpp — inside Runner::_runner() draw pass
DirtyFlag dirty = Runner::consume_dirty();   // atomic acquire, clears all bits
bool force_all  = Runner::is_dirty(dirty, DirtyFlag::All);

// Replace: if (force_redraw) Cpu::redraw = true;
// With:
if (force_all || Runner::is_dirty(dirty, DirtyFlag::Cpu)) {
    output += Cpu::draw(cpu, /*force_redraw=*/true, conf.no_update);
} else {
    output += Cpu::draw(cpu, /*force_redraw=*/false, conf.no_update);
}
```

**Integration at calcSizes():**

```cpp
// btop_draw.cpp — calcSizes() replaces per-box assigns + request_redraw()
// Before: Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true;
//         Runner::request_redraw();
// After:
Runner::mark_dirty(DirtyFlag::Cpu | DirtyFlag::Mem | DirtyFlag::Net | DirtyFlag::Proc | DirtyFlag::Gpu | DirtyFlag::Menu);
```

### Supporting Libraries (existing, no changes)

| Library | Version | Purpose | Notes for v1.6 |
|---------|---------|---------|----------------|
| **GoogleTest** | v1.17.0 | Unit tests for DirtyFlags logic | `dirty_flags` logic is pure bit manipulation — fully testable without terminal or runner. New test file `tests/test_dirty_flags.cpp`. |
| **TSan** | runtime sanitizer | Verify fetch_or/exchange ordering | `build-tsan/` already exists. The acquire/release ordering on the atomic is correct, but TSan will confirm no other shared-mutable-state race is introduced during migration. |

### Development Tools (existing, no changes)

| Tool | Purpose | Notes |
|------|---------|-------|
| **ASan + UBSan** | Detect any UB introduced during flag migration | Build config `build-asan/` already exists. Run after removing dead `Proc::resized` atomic. |
| **TSan** | Confirm atomic flag access has no races | Run before and after switching `pending_redraw` to `dirty_flags`. |

---

## Installation

No new dependencies for v1.6. All required headers (`<atomic>`, `<cstdint>`, `<type_traits>`) are part of the C++ standard library.

```bash
# Nothing to install — all technologies are:
# 1. C++ standard library (GCC 12+ / Clang 15+, already required)
# 2. Already present dependencies (GoogleTest v1.17.0 via FetchContent, nanobench)
```

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| `std::atomic<uint8_t>` with `fetch_or` / `exchange(0)` | `std::atomic<bool>` per box (current approach) | Never — the current approach requires N separate atomic operations to set all flags, and the flags can become inconsistent if one box is marked dirty between checking others. A single atomic uint8_t is strictly superior. |
| `std::atomic<uint8_t>` with `fetch_or` / `exchange(0)` | `std::bitset<8>` | Never for cross-thread use. `std::bitset` has no atomic operations at all. Its `operator[]`, `set()`, and `reset()` are not thread-safe. For a single-threaded context where the flags are set and consumed on the same thread with no cross-thread handoff, `std::bitset` would be appropriate but adds no value over a plain `uint8_t`. |
| `enum class DirtyFlag : uint8_t` | Plain `uint8_t` constants (`constexpr uint8_t CPU_DIRTY = 1u << 0`) | Only if the scoped enum concept is seen as overkill for this small flag set. The enum class prevents accidental mixing with unrelated integers, makes the intent explicit in function signatures, and is consistent with how the codebase uses `AppStateTag`, `RunnerStateTag`, `ThemeKey`, etc. Maintain consistency. |
| `fetch_or(release)` / `exchange(0, acquire)` | `fetch_or(seq_cst)` / `exchange(0, seq_cst)` | Never for this use case. `seq_cst` imposes a full memory fence on every operation, which is more expensive than release/acquire. Release/acquire is correct here: the flag setter synchronizes-with the flag consumer, establishing the happens-before relationship needed. On x86/TSO, acquire/release compile to the same instructions as relaxed for loads/stores — only the compiler fence differs. |
| `fetch_or(release)` / `exchange(0, acquire)` | `fetch_or(relaxed)` / `load(relaxed)` | Never. `relaxed` provides only atomicity (no torn read/write) but does NOT establish happens-before between threads. Using relaxed for a dirty flag notification means the runner thread may observe the flag set but not see the side effects that made the flag necessary (e.g., layout fields reset by `calcSizes`). This is a data race on those side effects. |
| `consume_dirty()` returns a snapshot (exchange to 0) | `test_and_clear` per bit | Only if partial consumption is needed — e.g., if the runner could skip Mem but not Cpu. In btop's runner, all visible boxes are drawn in sequence in a single pass; a single `exchange(0)` snapshot at the start of the draw pass is simpler and correct. If partial-draw capability is ever needed, the per-bit approach becomes viable. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **`std::bitset<8>`** | Not thread-safe. Has no `fetch_or`, no `exchange`, no `load` with memory ordering. `operator[]` and `set()` are data races when called from multiple threads. The current `menuMask bitset<8>` is single-threaded — that's why it's safe. The dirty flags cross from main thread to runner thread, which bitset cannot handle. | `std::atomic<uint8_t>` with `fetch_or` / `exchange(0)` |
| **`std::vector<bool>` for GPU redraw flags** | `vector<bool>` is a specialization that packs bits, but accessing individual elements is not thread-safe and there is no atomic interface. GPU panel redraw can be folded into a single `DirtyFlag::Gpu` bit (since `calcSizes()` already resizes and redraws all GPU panels together). If per-GPU-panel granularity is later required, the `uint8_t` can be widened to `uint16_t` with 8 GPU bits. | `DirtyFlag::Gpu` bit in `std::atomic<uint8_t>` |
| **`atomic<bool> pending_redraw` + `bool force_redraw` in `runner_conf`** (the thing being replaced) | Two-step indirection: main thread sets `pending_redraw`, runner folds it into `force_redraw` struct field — but `force_redraw` is a plain bool in a struct copied by value, so it cannot be updated after `thread_trigger()` without a race. A late `request_redraw()` between `thread_trigger()` and the draw pass can be lost if the pending_redraw fold already happened. The unified `dirty_flags` atomic is read at draw time, not at dispatch time, eliminating this window. | `Runner::dirty_flags` atomic with `consume_dirty()` at draw time |
| **`atomic<bool> Proc::resized`** | Declared in `btop_shared.hpp`, written in `btop_draw.cpp`, **never read anywhere in the codebase**. Dead code. Remove without replacement. | Remove entirely (v1.6 scope item) |
| **Setting per-namespace `redraw` bools from `calcSizes()`** | `calcSizes()` is called from the main thread. The per-namespace `bool redraw` fields (e.g., `Cpu::redraw`) are read from the runner thread. These are plain `bool` globals — reading from runner while main writes is undefined behavior (data race). The current code "works" only because `calcSizes()` calls `Runner::wait_idle()` first, coupling layout recomputation to runner lifecycle. Decoupling them (a v1.6 goal) removes this synchronization point and makes the race explicit. | `Runner::mark_dirty()` on the atomic, removing per-namespace bool fields |
| **Widening to `std::atomic<uint64_t>` preemptively** | Wasteful. 6 current flags fit in `uint8_t`. `uint8_t` atomics are lock-free on all targets. Widening adds no benefit unless the flag count exceeds 8 — at which point `uint16_t` is the right next step, not a 64-bit integer. | `uint8_t` now; widen to `uint16_t` if needed |

---

## Integration With Existing Architecture

### Enum-Indexed Array Pattern (from v1.0)

The codebase already uses enum-indexed arrays as the primary lookup pattern (ThemeKey, CpuField, MemField, NetDir, GpuField). `DirtyFlag` follows the same convention: enum class with typed underlying integer, explicit bit positions, no implicit conversion. The difference is that DirtyFlag values are bitmask bits, not array indices.

### Shadow Atomic Pattern (from v1.1/v1.2)

`Global::runner_state_tag` is a shadow atomic (`atomic<RunnerStateTag>`) that mirrors the main-thread `AppStateVar` for cross-thread reads. `Runner::dirty_flags` is analogous: a shadow atomic that cross-thread producers (main thread, signal handlers via event queue) set, and the runner thread consumes. The single-writer invariant (`transition_to()` is the only writer for `runner_state_tag`) has an analogue here: `mark_dirty()` is the only write path to `dirty_flags`.

### calcSizes() Decoupling (v1.6 goal)

Today `calcSizes()` couples layout recomputation and redraw forcing:
```cpp
// Today (coupled):
Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true;
Runner::request_redraw();
```

After migration:
```cpp
// After (decoupled):
// Layout recomputation (calcSizes body) runs here — no redraw implication
Runner::mark_dirty(DirtyFlag::Cpu | DirtyFlag::Mem | DirtyFlag::Net | DirtyFlag::Proc | DirtyFlag::Gpu | DirtyFlag::Menu);
// Runner reads dirty flags at draw time, not at dispatch time
```

This removes the semantic coupling between "I changed geometry" and "force the runner to redraw right now." `calcSizes()` can run without caring whether the runner is active.

### Memory Ordering in the Btop Threading Model

btop has a simple 2-thread model: main thread (writes flags via `mark_dirty`) and runner thread (reads and clears via `consume_dirty`). The `menu_open/close/clear` wrappers are main-thread only. There is no multi-producer scenario for the dirty flags (signal handlers route through the event queue to the main thread, which then calls `mark_dirty`). This simplicity justifies `memory_order_release` on producers and `memory_order_acquire` on the consumer — no need for `memory_order_seq_cst`.

---

## Version Compatibility

| Component | Version Requirement | Notes |
|-----------|---------------------|-------|
| `enum class E : uint8_t` | C++11 | Standard scoped enum with explicit underlying type; available in GCC 12+ / Clang 15+ |
| `std::atomic<uint8_t>` | C++11, `<atomic>` | `uint8_t` is an integer type; `is_always_lock_free` is true on x86 and ARM |
| `atomic<uint8_t>::fetch_or` | C++11, `<atomic>` | Member of `atomic<Integral>` specializations; confirmed in cppreference |
| `atomic<uint8_t>::exchange` | C++11, `<atomic>` | Available on all atomic specializations |
| `std::underlying_type_t<T>` | C++14 (`_t` alias form) | C++11 has `std::underlying_type<T>::type`; `_t` alias is C++14. Both GCC 12 and Clang 15 support C++14 as baseline. |
| `std::to_underlying(e)` | C++23 | Optional simplification of `static_cast<underlying_type_t<E>>(e)`. Use only if `btop`'s CMakeLists already targets C++23. Falls back to manual `static_cast` without any loss of correctness. |
| `[[nodiscard]]` on `consume_dirty` | C++17 | Prevents accidentally discarding the flag snapshot without acting on it. Already used in this codebase. |

---

## Sources

- [cppreference — `std::atomic<T>::fetch_or`](https://en.cppreference.com/w/cpp/atomic/atomic/fetch_or.html) — Confirmed `fetch_or` is a member of `atomic<Integral>` specializations; memory_order parameter documented. **HIGH confidence.**
- [cppreference — `std::memory_order`](https://en.cppreference.com/w/cpp/atomic/memory_order.html) — Release/acquire pattern for producer-consumer notification: "store in thread A tagged `memory_order_release` synchronizes-with load in thread B tagged `memory_order_acquire`." **HIGH confidence.**
- [cppreference — `std::bitset`](https://en.cppreference.com/w/cpp/utility/bitset.html) — Confirmed no atomic interface; thread safety requires external synchronization. **HIGH confidence.**
- [cppreference — `std::to_underlying`](https://en.cppreference.com/w/cpp/utility/to_underlying) — C++23, equivalent to `static_cast<std::underlying_type_t<Enum>>(e)`. **HIGH confidence.**
- [Andreas Fertig — C++20 Concepts applied: Safe bitmasks using scoped enums (2024)](https://andreasfertig.com/blog/2024/01/cpp20-concepts-applied/) — C++20 concept-gated operator overloads on scoped enums using `requires`; eliminates `decltype/enable_if` verbosity. **HIGH confidence** (official author blog, current).
- [voithos.io — Type-safe Enum Class Bit Flags](https://voithos.io/articles/type-safe-enum-class-bit-flags/) — `BitFlags<T>` wrapper pattern as an alternative to operator overloads; confirms the tradeoff between overloaded operators (simpler client code) and wrapper type (stronger type distinction). **MEDIUM confidence** (blog, independently corroborated by Fertig).
- [preshing.com — You Can Do Any Kind of Atomic Read-Modify-Write Operation](https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/) — CAS-loop as the general RMW primitive; `fetch_or` as a specialized single-instruction form where the operation is simple enough. Confirms `exchange(0)` is the correct consume-and-clear primitive. **HIGH confidence** (Jeff Preshing is the canonical reference for C++ memory ordering).
- [brilliantsugar.github.io — How I Learned to Stop Worrying and Love Juggling C++ Atomics](https://brilliantsugar.github.io/posts/how-i-learned-to-stop-worrying-and-love-juggling-c++-atomics/) — Dirty flag embedded in a pointer with `memory_order_acq_rel` on exchange; confirms the correctness problem with separate flag + pointer atomics and the superiority of a single atomic that carries both. **MEDIUM confidence** (blog, good reasoning).
- `src/btop.cpp`, `src/btop_draw.cpp`, `src/btop_shared.hpp`, `src/btop_state.hpp` — Direct codebase analysis. Identified all 7 redraw flag sites, the `pending_redraw → force_redraw` fold, `Proc::resized` dead code, and `calcSizes()` coupling. **HIGH confidence** (primary source).
- [WebSearch: `ATOMIC_INT_LOCK_FREE`, `is_always_lock_free` on x86/ARM] — Community consensus: `std::atomic<uint32_t>` (and smaller integers) `is_always_lock_free == true` on x86-32, x86-64, ARMv7, ARM64 — all btop target platforms. **MEDIUM confidence** (community + preshing; C++ standard only guarantees `atomic_flag` is always lock-free).

---

*Stack research for: btop++ v1.6 Unified Dirty-Flag Mechanism*
*Researched: 2026-03-03*
