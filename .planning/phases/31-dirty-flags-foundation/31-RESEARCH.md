# Phase 31: DirtyFlags Foundation - Research

**Researched:** 2026-03-07
**Domain:** C++ atomic bitflag infrastructure, dead code removal, naming cleanup
**Confidence:** HIGH

## Summary

Phase 31 introduces a `DirtyBit` enum and `PendingDirty` atomic accumulator as the foundational type for the v1.6 unified redraw system. The implementation is straightforward: a `uint32_t`-backed enum class with per-box bits, wrapped in a struct using `fetch_or` (mark) and `exchange` (take) on `std::atomic<uint32_t>`. The project already uses C++23, `std::atomic`, and memory ordering throughout the codebase, so this is well-trodden ground.

The cleanup tasks are surgical: `Proc::resized` is declared as `atomic<bool>` (btop_shared.hpp:721, btop_draw.cpp:2220) but never written to -- only read at btop_draw.cpp:2926 in a guard condition. The four `bool redraw` locals in btop_input.cpp (lines 355, 587, 618, 640) are simple renames. Both are low-risk mechanical changes.

**Primary recommendation:** Implement btop_dirty.hpp as a self-contained header with no dependencies on project internals, add a GTest file to the existing test suite, then perform the two cleanup tasks as separate commits.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FLAG-01 | DirtyBit enum with per-box bits (Cpu, Mem, Net, Proc, Gpu) plus ForceFullEmit | New enum class in btop_dirty.hpp, uint32_t underlying, one bit per box |
| FLAG-02 | PendingDirty struct with mark()/take() API using fetch_or/exchange | Wraps atomic<uint32_t>; mark() uses fetch_or with release, take() uses exchange with acquire |
| FLAG-03 | Single DirtyBit::Gpu covers all GPU panels | Current Gpu::redraw is vector<bool> (btop_shared.hpp:488); single bit replaces it in later phases |
| FLAG-04 | Unit tests verify bit operations, mark/take semantics, concurrent access | Add test_dirty.cpp to existing GTest suite in tests/ |
| CLEAN-01 | Dead Proc::resized atomic<bool> removed from declaration and read site | Declared btop_shared.hpp:721, defined btop_draw.cpp:2220, read btop_draw.cpp:2926 |
| CLEAN-02 | calcSizes() guard simplified after Proc::resized removal | Guard at btop_draw.cpp:2926 simplifies to just AppStateTag::Resizing check |
| CLEAN-03 | Local bool redraw vars in btop_input.cpp renamed to force_redraw | Four instances at lines 355, 587, 618, 640 |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++23 std::atomic | N/A | Lock-free atomic operations | Already used throughout codebase (CMakeLists.txt: CMAKE_CXX_STANDARD 23) |
| GTest | v1.17.0 | Unit testing | Already configured via FetchContent in tests/CMakeLists.txt |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ThreadSanitizer (TSan) | Compiler-bundled | Verify concurrent mark+take | CMake already has BTOP_SANITIZERS support |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Raw atomic<uint32_t> | std::bitset | bitset is not atomic; would need external lock |
| fetch_or/exchange | compare_exchange loop | CAS loop is unnecessary here; fetch_or is simpler and correct |
| enum class uint32_t | enum class uint8_t | uint32_t matches STATE.md decision and leaves room for future bits |

**Installation:**
No new dependencies. Everything uses existing project infrastructure.

## Architecture Patterns

### Recommended File Structure
```
src/
  btop_dirty.hpp       # NEW: DirtyBit enum + PendingDirty struct (header-only)
tests/
  test_dirty.cpp       # NEW: GTest file for dirty flag operations
```

### Pattern 1: Atomic Bitmask Accumulator
**What:** A struct wrapping `atomic<uint32_t>` where producers `mark()` bits via `fetch_or` and a single consumer `take()`s the accumulated mask via `exchange(0)`.
**When to use:** Multiple threads/sites need to signal "something changed" and one consumer needs to atomically snapshot and clear.
**Example:**
```cpp
// btop_dirty.hpp
#pragma once
#include <atomic>
#include <cstdint>

enum class DirtyBit : uint32_t {
    Cpu          = 1u << 0,
    Mem          = 1u << 1,
    Net          = 1u << 2,
    Proc         = 1u << 3,
    Gpu          = 1u << 4,
    ForceFullEmit = 1u << 5,
};

// Enable bitwise operators for DirtyBit
constexpr DirtyBit operator|(DirtyBit a, DirtyBit b) {
    return static_cast<DirtyBit>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr DirtyBit operator&(DirtyBit a, DirtyBit b) {
    return static_cast<DirtyBit>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
constexpr bool has_bit(DirtyBit mask, DirtyBit bit) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(bit)) != 0;
}

// Convenience: all per-box bits OR'd together
constexpr DirtyBit DirtyAll = DirtyBit::Cpu | DirtyBit::Mem | DirtyBit::Net
                            | DirtyBit::Proc | DirtyBit::Gpu;

struct PendingDirty {
    // Mark one or more bits as dirty (thread-safe, lock-free)
    void mark(DirtyBit bits) {
        bits_.fetch_or(static_cast<uint32_t>(bits), std::memory_order_release);
    }

    // Atomically take all accumulated dirty bits and clear (single consumer)
    DirtyBit take() {
        return static_cast<DirtyBit>(
            bits_.exchange(0, std::memory_order_acquire));
    }

    // Peek without clearing (for diagnostics/testing only)
    DirtyBit peek() const {
        return static_cast<DirtyBit>(
            bits_.load(std::memory_order_acquire));
    }

private:
    std::atomic<uint32_t> bits_{0};
};
```

### Pattern 2: Guard Simplification (CLEAN-01/02)
**What:** Remove dead `Proc::resized` and simplify the calcSizes() guard.
**Current code (btop_draw.cpp:2926):**
```cpp
if (not (Proc::resized or Global::app_state.load() == AppStateTag::Resizing)) {
    Proc::p_counters.clear();
    Proc::p_graphs.clear();
}
```
**After removal:**
```cpp
if (Global::app_state.load() != AppStateTag::Resizing) {
    Proc::p_counters.clear();
    Proc::p_graphs.clear();
}
```
`Proc::resized` is never set to `true` anywhere, so its presence in the guard is dead code -- removing it is semantically identical.

### Anti-Patterns to Avoid
- **Adding PendingDirty as a global in this phase:** Phase 31 only creates the type and tests. The global instance and wiring happen in Phase 32 (WIRE-01).
- **Modifying draw function signatures now:** Phase 31 is foundation only. Draw signatures change in Phase 34.
- **Using relaxed ordering for both mark and take:** The producer needs release (so the dirty state is visible), the consumer needs acquire (so it sees all prior writes). This matches the existing `pending_redraw` pattern in btop.cpp.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Bitwise operators for enum class | Manual cast at every call site | Free-standing operator overloads defined once in header | Error-prone, verbose, easy to forget cast |
| Thread-safe accumulator | mutex + bool set | atomic fetch_or + exchange | Lock-free, single cache line, already proven pattern in codebase |

**Key insight:** The entire PendingDirty implementation is ~30 lines. Its value is in being tested and having a clean API that later phases wire in.

## Common Pitfalls

### Pitfall 1: Memory Ordering Mismatch
**What goes wrong:** Using `relaxed` for both mark and take can cause the consumer to miss bits on weakly-ordered architectures (ARM).
**Why it happens:** x86 has strong ordering so tests pass even with wrong ordering.
**How to avoid:** Use `memory_order_release` for mark (fetch_or) and `memory_order_acquire` for take (exchange). This matches the project's existing pattern (see btop.cpp:903).
**Warning signs:** TSan reports, intermittent missed redraws on ARM-based Macs.

### Pitfall 2: Forgetting to Remove All Proc::resized References
**What goes wrong:** Removing the declaration but missing the definition or read site causes compile errors (good) or leaving dead code (bad).
**Why it happens:** Proc::resized appears in 3 places across 2 files.
**How to avoid:** Complete list: btop_shared.hpp:721 (extern decl), btop_draw.cpp:2220 (definition), btop_draw.cpp:2926 (read site). Remove all three.
**Warning signs:** Grep for `resized` in src/ after removal -- only btop_tools.hpp:143 (terminal resize, unrelated) and btop.cpp:138 (comment, unrelated) should remain.

### Pitfall 3: Renaming redraw but Missing a Usage
**What goes wrong:** Renaming the declaration but not all uses within the same scope causes compile error.
**Why it happens:** Each `bool redraw` has a limited scope within its `if (Xxx::shown)` block, but the variable is used multiple times within.
**How to avoid:** For each of the 4 blocks, rename the declaration AND all uses within that block. The variable is used in: assignment, condition checks, and the `Runner::run()` call.
**Warning signs:** Compile errors are the safety net here.

### Pitfall 4: Accidentally Wiring PendingDirty Into Runner
**What goes wrong:** Scope creep -- replacing `pending_redraw` or per-box bools in this phase.
**Why it happens:** It's tempting to use the new type immediately.
**How to avoid:** Phase 31 creates the type and tests ONLY. No functional changes to the runner or draw paths. Phase 32 handles wiring.

## Code Examples

### Test Pattern (based on existing test_app_state.cpp style)
```cpp
// test_dirty.cpp
#include <gtest/gtest.h>
#include "btop_dirty.hpp"
#include <thread>
#include <vector>

TEST(DirtyBit, MarkSingleBit) {
    PendingDirty pd;
    pd.mark(DirtyBit::Cpu);
    auto taken = pd.take();
    EXPECT_TRUE(has_bit(taken, DirtyBit::Cpu));
    EXPECT_FALSE(has_bit(taken, DirtyBit::Mem));
}

TEST(DirtyBit, MarkMultipleBits) {
    PendingDirty pd;
    pd.mark(DirtyBit::Cpu);
    pd.mark(DirtyBit::Mem);
    auto taken = pd.take();
    EXPECT_TRUE(has_bit(taken, DirtyBit::Cpu));
    EXPECT_TRUE(has_bit(taken, DirtyBit::Mem));
}

TEST(DirtyBit, TakeClears) {
    PendingDirty pd;
    pd.mark(DirtyBit::Proc);
    pd.take();
    auto second = pd.take();
    EXPECT_EQ(static_cast<uint32_t>(second), 0u);
}

TEST(DirtyBit, MarkAll) {
    PendingDirty pd;
    pd.mark(DirtyAll);
    auto taken = pd.take();
    EXPECT_TRUE(has_bit(taken, DirtyBit::Cpu));
    EXPECT_TRUE(has_bit(taken, DirtyBit::Mem));
    EXPECT_TRUE(has_bit(taken, DirtyBit::Net));
    EXPECT_TRUE(has_bit(taken, DirtyBit::Proc));
    EXPECT_TRUE(has_bit(taken, DirtyBit::Gpu));
}

TEST(DirtyBit, ForceFullEmitSeparateFromBoxBits) {
    PendingDirty pd;
    pd.mark(DirtyBit::ForceFullEmit);
    auto taken = pd.take();
    EXPECT_TRUE(has_bit(taken, DirtyBit::ForceFullEmit));
    EXPECT_FALSE(has_bit(taken, DirtyBit::Cpu));
}

TEST(DirtyBit, ConcurrentMarkAndTake) {
    PendingDirty pd;
    constexpr int iterations = 10000;

    std::thread producer([&] {
        for (int i = 0; i < iterations; ++i) {
            pd.mark(DirtyBit::Cpu | DirtyBit::Mem);
        }
    });

    uint32_t total_taken = 0;
    for (int i = 0; i < iterations; ++i) {
        total_taken |= static_cast<uint32_t>(pd.take());
    }
    producer.join();
    // Drain any remaining
    total_taken |= static_cast<uint32_t>(pd.take());

    // At least some marks should have been observed
    EXPECT_TRUE(total_taken & static_cast<uint32_t>(DirtyBit::Cpu));
    EXPECT_TRUE(total_taken & static_cast<uint32_t>(DirtyBit::Mem));
}
```

### Test CMakeLists.txt Update
```cmake
# Add test_dirty.cpp to the btop_test executable
add_executable(btop_test tools.cpp test_uncolor.cpp test_ring_buffer.cpp test_enum_arrays.cpp test_write_stdout.cpp test_graph_cache.cpp test_input_fsm.cpp test_menu_pda.cpp test_pda_invariants.cpp test_screen_buffer.cpp test_app_state.cpp test_events.cpp test_transitions.cpp test_runner_state.cpp test_dirty.cpp)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| C++20 fetch_or not available | C++26 added fetch_or to atomic<integral> | C++26 (but GCC/Clang support since ~2022 as extension) | Project uses C++23; fetch_or is available on all target compilers (GCC 12+, Clang 14+, Apple Clang 15+) |
| Per-box bool + atomic<bool> | Unified atomic bitmask | v1.6 (this milestone) | Eliminates 6+ scattered flags with one 4-byte atomic |

**Note on fetch_or availability:** `std::atomic<uint32_t>::fetch_or` is formally C++26 but has been available in GCC since ~12 and Clang since ~14 as an extension of the existing fetch_add/fetch_sub family. The project targets C++23 and already compiles with these compilers. If a platform lacks it, `fetch_or` can be trivially implemented via CAS loop, but this is unlikely to be needed.

## Open Questions

1. **fetch_or compiler support breadth**
   - What we know: GCC 12+ and Clang 14+ support it. Project uses C++23. macOS/Linux/FreeBSD all targeted.
   - What's unclear: Whether the oldest supported compiler version on FreeBSD has it.
   - Recommendation: Provide a CAS-loop fallback `#if` guard if compile fails, but expect it to work. Test in CI.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build-test && ctest --test-dir . -R DirtyBit --output-on-failure` |
| Full suite command | `cd build-test && ctest --test-dir . --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FLAG-01 | DirtyBit enum has correct bit values | unit | `ctest -R DirtyBit --output-on-failure` | No -- Wave 0 |
| FLAG-02 | mark()/take() API works correctly | unit | `ctest -R DirtyBit --output-on-failure` | No -- Wave 0 |
| FLAG-03 | Single Gpu bit covers all panels | unit | `ctest -R DirtyBit --output-on-failure` | No -- Wave 0 |
| FLAG-04 | Concurrent mark+take under TSan | unit+tsan | `ctest -R DirtyBit --output-on-failure` (with TSan build) | No -- Wave 0 |
| CLEAN-01 | Proc::resized removed, compiles clean | build | `cmake --build build-test` | N/A (compile check) |
| CLEAN-02 | calcSizes() guard simplified | build+manual | `cmake --build build-test` | N/A (compile check) |
| CLEAN-03 | redraw renamed to force_redraw | build | `cmake --build build-test` | N/A (compile check) |

### Sampling Rate
- **Per task commit:** `cd build-test && cmake --build . && ctest --output-on-failure`
- **Per wave merge:** Full test suite + TSan build
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_dirty.cpp` -- covers FLAG-01, FLAG-02, FLAG-03, FLAG-04
- [ ] `tests/CMakeLists.txt` update -- add test_dirty.cpp to btop_test executable

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection of btop source files (btop_shared.hpp, btop_draw.cpp, btop_input.cpp, btop.cpp)
- tests/CMakeLists.txt -- existing test infrastructure
- .planning/REQUIREMENTS.md -- requirement definitions
- .planning/STATE.md -- locked decisions (atomic<uint32_t>, fetch_or/exchange, release/acquire)

### Secondary (MEDIUM confidence)
- C++ reference for std::atomic::fetch_or -- formally C++26 but widely available as compiler extension

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - uses only existing project infrastructure (C++23, GTest)
- Architecture: HIGH - pattern is simple (atomic bitmask), well-understood, matches existing codebase patterns
- Pitfalls: HIGH - all based on direct code inspection of specific line numbers

**Research date:** 2026-03-07
**Valid until:** 2026-04-07 (stable domain, no external dependencies)
