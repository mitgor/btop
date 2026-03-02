# Phase 18: Test & Doc Hygiene - Research

**Researched:** 2026-03-01
**Domain:** C++ test fix (RingBuffer) + documentation hygiene (stale VERIFICATION.md)
**Confidence:** HIGH

## Summary

Phase 18 addresses two independent hygiene items that accumulated during v1.1 development. The first is a pre-existing test failure (`RingBuffer.PushBackOnZeroCapacity`, test #34) caused by a mismatch between the test expectation (push_back on zero-capacity is a no-op) and the implementation (which auto-resizes to capacity 1). The second is stale content in Phase 12's `12-VERIFICATION.md` that references `dispatch_state()`, `state_tag`, and `process_signal_event` infrastructure that was replaced by Phase 13's `std::variant`-based dispatch.

Both issues are straightforward, well-understood, and low-risk. The RingBuffer fix is a single-line change (replace `resize(1)` with `return`). The VERIFICATION.md fix is an annotation-only change (add a staleness note without modifying original content). A plan (18-01-PLAN.md) already exists and is well-specified.

**Primary recommendation:** Execute 18-01-PLAN.md as written. The fix is one line in `push_back()`, verified across three build configs (normal, ASan+UBSan, TSan), plus a staleness annotation on 12-VERIFICATION.md.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| HYGN-01 | RingBuffer.PushBackOnZeroCapacity test passes (fix pre-existing failure) | RingBuffer::push_back() at btop_shared.hpp:255-267 auto-resizes on zero capacity instead of returning. Fix: replace `resize(1)` with `return`. Test at test_ring_buffer.cpp:268-272. No production code depends on auto-resize (all callers resize explicitly before pushing). |
| HYGN-02 | Stale VERIFICATION.md references to removed Phase 12 infrastructure cleaned up | 12-VERIFICATION.md lines 36-38, 58, 85 reference dispatch_state(), state_tag, process_signal_event -- all replaced by Phase 13. Fix: add staleness annotation blockquote, preserve original content. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| GoogleTest | v1.17.0 | Test framework | Already in use via FetchContent in tests/CMakeLists.txt |
| CMake | 3.25+ | Build system | Project's existing build system with CTest integration |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ASan+UBSan | Clang built-in | Memory/undefined behavior detection | Sanitizer verification sweep |
| TSan | Clang built-in | Thread safety detection | Thread sanitizer verification sweep |

### Alternatives Considered

None. This phase uses only existing infrastructure. No new libraries or tools needed.

## Architecture Patterns

### Pattern 1: Defensive No-Op on Invalid State
**What:** Container methods that receive invalid state (zero capacity, empty buffer) return silently rather than crashing or allocating.
**When to use:** When the container can be in a "not yet initialized" state and callers may invoke methods before initialization.
**Example:**
```cpp
// Source: src/btop_shared.hpp - existing pattern in pop_front (line 269)
void pop_front() {
    if (size_ == 0) return;  // no-op on empty
    head_ = (head_ + 1) % capacity_;
    --size_;
}

// Same pattern needed in push_back (line 255):
void push_back(const T& value) {
    if (capacity_ == 0) return;  // no-op when no storage allocated
    // ... rest of implementation
}
```

This pattern is already used consistently across the RingBuffer class:
- `operator[]` returns `default_val` on zero capacity/size
- `front()`, `back()` return `default_val` on zero capacity/size
- `pop_front()` returns on empty
- `push_back()` is the sole outlier (auto-resizes instead of returning)

### Pattern 2: Annotation-Only Documentation Updates
**What:** When documentation describes infrastructure that was later replaced, annotate rather than rewrite. Preserve the original verification as historical record.
**When to use:** When a VERIFICATION.md describes artifacts that were accurate when written but have since been superseded by later phases.
**Example:**
```markdown
> **Staleness Note (Phase 18 Hygiene):** This verification was accurate at the time Phase 12 completed.
> Phase 13 subsequently replaced the following infrastructure:
> - `dispatch_state()` template (replaced by variant-based dispatch)
> - `state_tag` namespace (replaced by `state::` structs)
```

### Anti-Patterns to Avoid
- **Rewriting historical verification documents:** The original 12-VERIFICATION.md was accurate when written. Rewriting it would lose the historical record and could introduce inaccuracies about what was verified at Phase 12 time.
- **Modifying the test to match the bug:** The test expectation (push_back on zero capacity is a no-op) is correct. The implementation is wrong. Do not modify the test.
- **Adding auto-resize fallback elsewhere:** All production code explicitly calls `resize()` before `push_back()`. Adding implicit resize would mask initialization bugs.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Ring buffer container | Custom ring buffer | Existing RingBuffer<T> in btop_shared.hpp | Already fully tested with 21 test cases |
| Test framework | Custom assertions | GoogleTest (gtest) | Already integrated, provides ASan/TSan compatibility |

**Key insight:** This phase does not require building anything new. It fixes one line of existing code and annotates one existing document.

## Common Pitfalls

### Pitfall 1: Breaking Production Code with the Fix
**What goes wrong:** Changing push_back behavior could break production code that relies on auto-resize.
**Why it happens:** Defensive coding added auto-resize as a safety net, creating concern about removing it.
**How to avoid:** Verified by grep: no production code path calls push_back on a zero-capacity buffer. All RingBuffer instances are either (a) constructed with explicit capacity, (b) default-constructed then resize() called before push_back, or (c) used in Draw where capacity is set during initialization. The auto-resize is unreachable in practice.
**Warning signs:** If any test other than PushBackOnZeroCapacity fails after the change, investigate immediately.

### Pitfall 2: Modifying the Wrong Verification File
**What goes wrong:** HYGN-02 specifies Phase 12's VERIFICATION.md. Editing other phases' VERIFICATION.md files would be out of scope.
**Why it happens:** Multiple VERIFICATION.md files reference related concepts (e.g., Phase 14's references to `runner_state_tag` look similar but are different infrastructure that still exists).
**How to avoid:** Only modify `.planning/milestones/v1.1-phases/12-extract-transitions/12-VERIFICATION.md`. The `runner_state_tag` references in Phase 14 VERIFICATION.md are correct and current (that infrastructure was NOT removed).
**Warning signs:** Diff shows changes to any file other than btop_shared.hpp and 12-VERIFICATION.md.

### Pitfall 3: Incomplete Sanitizer Verification
**What goes wrong:** Fix passes in normal build but triggers sanitizer findings.
**Why it happens:** The RingBuffer change is trivial (early return), but sanitizers could catch pre-existing issues unmasked by the change.
**How to avoid:** Run all three build configs explicitly: (1) normal build, (2) ASan+UBSan (`-DBTOP_SANITIZER=address,undefined`), (3) TSan (`-DBTOP_SANITIZER=thread`). All 266 tests must pass in each.
**Warning signs:** Any test failure or sanitizer warning in any build config.

## Code Examples

### The Fix: RingBuffer::push_back Zero-Capacity Guard

```cpp
// Source: src/btop_shared.hpp, line 255-267
// BEFORE (buggy):
void push_back(const T& value) {
    if (capacity_ == 0) {
        resize(1);       // BUG: auto-resizes, test expects no-op
    }
    if (size_ < capacity_) {
        data_[(head_ + size_) % capacity_] = value;
        ++size_;
    } else {
        data_[head_] = value;
        head_ = (head_ + 1) % capacity_;
    }
}

// AFTER (fixed):
void push_back(const T& value) {
    if (capacity_ == 0) return;  // no-op, matches pop_front/operator[]/front/back pattern
    if (size_ < capacity_) {
        data_[(head_ + size_) % capacity_] = value;
        ++size_;
    } else {
        data_[head_] = value;
        head_ = (head_ + 1) % capacity_;
    }
}
```

### Build and Test Commands

```bash
# Normal build and test
cmake --build build-test --target btop_test && cd build-test && ctest --output-on-failure

# ASan+UBSan build and test
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DBTOP_SANITIZER=address,undefined && \
cmake --build build-asan --target btop_test && cd build-asan && ctest --output-on-failure

# TSan build and test
cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DBTOP_SANITIZER=thread && \
cmake --build build-tsan --target btop_test && cd build-tsan && ctest --output-on-failure
```

### Stale References in 12-VERIFICATION.md

Lines containing stale references (infrastructure replaced by Phase 13):
- **Line 36:** `dispatch_event()` at src/btop.cpp:348 composes `dispatch_state()` + `std::visit()` -- `dispatch_state()` no longer exists
- **Line 37:** `dispatch_state()` converts AppState enum to state_tag types -- `state_tag` namespace no longer exists
- **Line 38:** std::visit resolves event type, combined with state_tag -- stale mechanism
- **Line 58:** TRANS-02 evidence cites `dispatch_state + std::visit` -- replaced by `std::variant`-based two-argument dispatch_event
- **Line 85:** Artifacts table lists `state_tag, dispatch_state, dispatch_event declaration` in btop_events.hpp -- `state_tag` and `dispatch_state` no longer exist there

Phase 13 replaced this with: `AppStateVar dispatch_event(const AppStateVar&, const AppEvent&)` using a two-variant `std::visit` call (no `state_tag` intermediate). See 13-VERIFICATION.md line 63 for confirmation.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `dispatch_state()` + state_tag + std::visit | `std::variant`-based dispatch_event(AppStateVar, AppEvent) | Phase 13 (2026-02-28) | Phase 12 VERIFICATION references stale |
| push_back auto-resizes on zero capacity | push_back should be no-op on zero capacity (pending fix) | Phase 4 introduced bug | Test #34 fails since Phase 4 |

**Current state confirmed by running tests:**
- Test suite: 265/266 pass (test #34 is the sole failure)
- `dispatch_state()` and `state_tag` namespace: confirmed absent from all src/ files (grep returns zero matches)
- `runner_state_tag`: still exists and is correct (different concept, introduced in Phase 14)

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cmake --build build-test --target btop_test && cd build-test && ctest --output-on-failure -R "RingBuffer"` |
| Full suite command | `cmake --build build-test --target btop_test && cd build-test && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| HYGN-01 | push_back on zero-capacity RingBuffer is a no-op | unit | `ctest --output-on-failure -R "RingBuffer.PushBackOnZeroCapacity"` | Yes (test_ring_buffer.cpp:268) |
| HYGN-01 | Full suite 266/266 pass | integration | `ctest --output-on-failure` | Yes |
| HYGN-01 | ASan+UBSan clean | sanitizer | Build with `-DBTOP_SANITIZER=address,undefined`, run ctest | Build config exists |
| HYGN-01 | TSan clean | sanitizer | Build with `-DBTOP_SANITIZER=thread`, run ctest | Build config exists |
| HYGN-02 | Staleness note present in 12-VERIFICATION.md | grep check | `grep "Staleness Note" .planning/milestones/v1.1-phases/12-extract-transitions/12-VERIFICATION.md` | File exists, annotation pending |

### Sampling Rate
- **Per task commit:** `ctest --output-on-failure -R "RingBuffer.PushBackOnZeroCapacity"` (< 1 second)
- **Per wave merge:** `ctest --output-on-failure` (full suite, ~3 seconds)
- **Phase gate:** Full suite green in all three build configs (normal + ASan+UBSan + TSan)

### Wave 0 Gaps

None. Existing test infrastructure covers all phase requirements. The failing test already exists and correctly specifies the expected behavior. No new test files, fixtures, or framework configuration needed.

## Open Questions

None. Both issues are fully understood:

1. **HYGN-01:** Root cause identified (auto-resize at btop_shared.hpp:256-258), fix known (replace with early return), safety verified (no production code relies on auto-resize).
2. **HYGN-02:** Stale lines identified (12-VERIFICATION.md lines 36-38, 58, 85), replacement infrastructure confirmed (Phase 13 variant-based dispatch), annotation strategy specified (blockquote note, preserve original content).

## Sources

### Primary (HIGH confidence)
- `src/btop_shared.hpp` lines 255-267 -- direct inspection of RingBuffer::push_back implementation
- `tests/test_ring_buffer.cpp` lines 268-272 -- direct inspection of failing test
- `.planning/milestones/v1.1-phases/12-extract-transitions/12-VERIFICATION.md` -- direct inspection of stale references
- `.planning/milestones/v1.1-phases/13-type-safe-states/13-VERIFICATION.md` -- confirms Phase 13 replacements
- Live test run: `ctest --output-on-failure` -- confirmed 265/266 pass, test #34 fails
- Live grep: `dispatch_state` and `state_tag` namespace absent from src/ -- confirmed stale

### Secondary (MEDIUM confidence)
- Production RingBuffer usage patterns (grep across src/*.cpp) -- confirmed no caller depends on auto-resize

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - uses only existing project infrastructure (GoogleTest, CMake, sanitizers)
- Architecture: HIGH - single-line fix follows existing defensive no-op pattern in the same class
- Pitfalls: HIGH - failure mode is well-understood (no production code depends on auto-resize, verified by grep)

**Research date:** 2026-03-01
**Valid until:** Indefinite (this is a bug fix, not a moving-target technology decision)

**Note:** An execution plan (18-01-PLAN.md) already exists in the phase directory and is well-specified with correct fix details, build commands, and verification steps. No plan changes are needed.
