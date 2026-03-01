# Phase 15: Verification - Research

**Researched:** 2026-03-01
**Domain:** C++ FSM unit testing, sanitizer integration (ASan/UBSan/TSan), behavioral verification
**Confidence:** HIGH

## Summary

Phase 15 is a verification-only phase. All code changes from Phases 10-14 are complete (App FSM + Runner FSM). The work here is writing comprehensive unit tests for both FSMs, running sanitizer sweeps, and confirming behavioral equivalence. No new library choices or architectural patterns are needed -- the test infrastructure (GoogleTest v1.17.0 via FetchContent), build system (CMake with BTOP_SANITIZER), and sanitizer builds (build-asan, build-tsan) already exist from earlier phases.

The primary challenge is achieving full (state, event) coverage. The App FSM has 6 states x 8 events = 48 pairs. The Runner FSM has 4 states with imperative transitions (not event-driven). Existing tests in `test_transitions.cpp` cover approximately 20 of the 48 App FSM pairs. The Runner FSM's `test_runner_state.cpp` covers struct/variant mechanics but NOT transition logic. The gap is significant: ~28 App FSM pairs and the entire Runner FSM transition logic are untested.

For sanitizer sweeps, the CMake build system already supports `BTOP_SANITIZER` with `address,undefined` and `thread` configurations. Existing build-asan and build-tsan directories are configured but built without tests (`BUILD_TESTING=OFF`). The phase needs to rebuild these with `BUILD_TESTING=ON` and run the test suite under sanitizers, plus exercise the application itself under sanitizers to stress cross-thread interactions.

**Primary recommendation:** Extend existing test files with missing (state, event) pair coverage, add Runner FSM transition tests as a new file or extend test_runner_state.cpp, then rebuild with sanitizers enabled and run both unit tests and a short interactive exercise.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| VERIFY-01 | Unit tests for App FSM transitions (each state+event pair tested) | 48 pairs enumerated; existing test_transitions.cpp covers ~20 pairs; gap analysis in Architecture Patterns section |
| VERIFY-02 | Unit tests for Runner FSM transitions | Runner state machine is imperative (not event-driven); transition tests need to verify RunnerStateTag atomic updates at each lifecycle stage; test_runner_state.cpp needs transition logic tests |
| VERIFY-03 | ASan/UBSan sweep confirms zero memory/UB regressions | CMake BTOP_SANITIZER already supports `address,undefined`; build-asan exists but needs BUILD_TESTING=ON rebuild |
| VERIFY-04 | TSan sweep confirms zero data race regressions | CMake BTOP_SANITIZER already supports `thread`; build-tsan exists but needs BUILD_TESTING=ON rebuild |
| VERIFY-05 | All existing functionality unchanged | Behavioral verification via manual smoke test + ensuring all existing tests pass under sanitizers |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| GoogleTest | v1.17.0 | Unit testing framework | Already used, fetched via FetchContent |
| CMake | 3.25+ | Build system with sanitizer flags | BTOP_SANITIZER variable already implemented |
| Apple Clang | 17.0.0 | Compiler with ASan/UBSan/TSan support | Project's default compiler on macOS |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| CTest | (bundled) | Test discovery and execution | `ctest --test-dir build-test` for running all tests |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| GoogleTest | Catch2 | Project already uses GoogleTest; no reason to switch |

**Build commands (already established):**
```bash
# Regular test build
cmake -B build-test -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test -j$(sysctl -n hw.ncpu)
ctest --test-dir build-test --output-on-failure

# ASan+UBSan test build
cmake -B build-asan -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DBTOP_SANITIZER=address,undefined
cmake --build build-asan -j$(sysctl -n hw.ncpu)
ctest --test-dir build-asan --output-on-failure

# TSan test build
cmake -B build-tsan -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DBTOP_SANITIZER=thread
cmake --build build-tsan -j$(sysctl -n hw.ncpu)
ctest --test-dir build-tsan --output-on-failure
```

## Architecture Patterns

### App FSM State-Event Matrix (Complete)

The App FSM uses `std::visit` two-variant dispatch. Each (state, event) pair either has a specific `on_event` overload or falls through to the catch-all `on_event(const auto& s, const auto&) { return s; }`.

**6 states:** Running, Resizing, Reloading, Sleeping, Quitting, Error
**8 events:** TimerTick, Resize, Quit, Sleep, Resume, Reload, ThreadError, KeyInput

| State \ Event | TimerTick | Resize | Quit | Sleep | Resume | Reload | ThreadError | KeyInput |
|---------------|-----------|--------|------|-------|--------|--------|-------------|----------|
| **Running** | Running(self) | Resizing | Quitting(code) | Sleeping | Running(self) | Reloading | Error("Thread error") | Running(self) |
| **Resizing** | Resizing | Resizing | Quitting(code) | Resizing | Resizing | Resizing | Resizing | Resizing |
| **Reloading** | Reloading | Reloading | Quitting(code) | Reloading | Reloading | Reloading | Reloading | Reloading |
| **Sleeping** | Sleeping | Sleeping | Quitting(code) | Sleeping | **Resizing** | Sleeping | Sleeping | Sleeping |
| **Quitting** | Quitting | Quitting | Quitting | Quitting | Quitting | Quitting | Quitting | Quitting |
| **Error** | Error | Error | Error | Error | Error | Error | Error | Error |

**IMPORTANT clarifications for the matrix:**
- The catch-all covers all non-Running state pairs EXCEPT Sleeping+Resume (which routes to Resizing).
- Note: The catch-all also absorbs `Quit` events for non-Running states. However, looking at the on_event overloads, only `on_event(Running, Quit)` exists. For Resizing+Quit, Reloading+Quit, and Sleeping+Quit, the catch-all returns the current state. This means the Quit signal is only effective from Running state. This IS the intended behavior -- the state chain (Reloading->Resizing->Running) completes before the next event drain, so Quit events arrive during Running.
- Quitting and Error are absorbing (terminal) states: all events return the same state.

### Existing Test Coverage (test_transitions.cpp)

Already tested (~20 pairs):
- Running + {Quit, Sleep, Resume, Resize, Reload, TimerTick, KeyInput, ThreadError} = 8 pairs
- Quitting + {Resize, Reload, TimerTick, KeyInput} = 4 pairs (preserved)
- Error + {Resize, Reload, TimerTick, KeyInput, Resume, Sleep} = 6 pairs (preserved)
- Resizing + Reload = 1 pair (preserved)
- Reloading + Resize = 1 pair (preserved)
- Sleeping + Resize = 1 pair (preserved)
- Cross-state dispatch = 1 (AllSixStatesRouteCorrectly with TimerTick)

**Gap analysis (~28 missing pairs):**
- Resizing + {TimerTick, Quit, Sleep, Resume, ThreadError, KeyInput, Resize} = 7 pairs (note: Resize on Resizing is self-transition, interesting edge case)
- Reloading + {TimerTick, Quit, Sleep, Resume, ThreadError, KeyInput, Reload} = 7 pairs
- Sleeping + {TimerTick, Quit, Sleep, Resume(already partially covered via AllSixStates), ThreadError, KeyInput, Reload} = 7 pairs (Resume already covered by separate test? No -- the specific Sleeping+Resume->Resizing is NOT tested individually)
- Quitting + {Sleep, Resume, ThreadError, Quit} = 4 pairs
- Error + {Quit, ThreadError, Error(self?)} = 3 pairs (Error + ThreadError important)

### Runner FSM Transition Pattern

The Runner FSM is NOT event-driven. It uses imperative state transitions in the `_runner()` thread function:

```
Loop:
  1. Set Idle + store RunnerStateTag::Idle + notify_all
  2. thread_wait() (semaphore acquire)
  3. Safety check: if is_active() -> Error
  4. Guard: if is_stopping() or Resizing -> continue to Idle
  5. Set Collecting + store RunnerStateTag::Collecting + notify_all
  6. ... collect data ...
  7. Exception guard: on exception -> set Error + store Stopping
  8. Guard: if is_stopping() -> continue to Idle
  9. Set Drawing + store RunnerStateTag::Drawing
  10. ... render output ...
  11. Loop back to Idle
```

**Valid Runner transitions:**
1. Idle -> Collecting (normal start after semaphore release)
2. Idle -> Idle (stopping/resizing guard, loop back)
3. Collecting -> Drawing (collection complete, no stopping)
4. Collecting -> Stopping (exception during collection)
5. Collecting -> Idle (is_stopping check, continue)
6. Drawing -> Idle (drawing complete, loop back)
7. Any -> Stopping (external Runner::stop() call)

**Testable aspects (without running the full thread):**
- RunnerStateTag atomic transitions mirror RunnerStateVar changes
- to_runner_tag() correctness (already tested)
- is_active() returns true for Collecting and Drawing only
- is_stopping() returns true for Stopping only
- wait_idle() returns when state is Idle or Stopping
- Runner::stop() sets Stopping state

### Pattern: Test Organization

**Use test suites matching the FSM structure:**
```
TEST(AppFsmTransition, <State><Event><ExpectedResult>)   // e.g., ResizingQuit_PreservesState
TEST(RunnerFsmTransition, <Transition>)                   // e.g., IdleToCollecting
TEST(RunnerFsmGuard, <GuardCondition>)                    // e.g., StoppingGuardSkipsCollecting
```

### Anti-Patterns to Avoid
- **Testing implementation details instead of behavior:** Don't test internal RunnerStateVar -- test the observable RunnerStateTag atomic and the is_active()/is_stopping()/wait_idle() query functions.
- **Coupling tests to btop globals:** Existing tests wisely use local atomic/variant instances. Continue this pattern. Do NOT depend on global state from btop.cpp.
- **Ignoring catch-all coverage:** The catch-all is load-bearing. Test representative non-Running state+event pairs to ensure the catch-all preserves state correctly.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Test framework | Custom test harness | GoogleTest (already in project) | Mature, CTest integration, test discovery |
| Sanitizer flags | Manual compiler flags | CMake BTOP_SANITIZER variable | Already implemented, handles both compile and link flags |
| Test matrix generation | Manual test list | Parameterized tests (TEST_P) or systematic naming | 48 pairs are systematic; use consistent naming for audit |

**Key insight:** The verification phase adds NO new production code. All tooling exists. The work is purely additive test code + build configurations.

## Common Pitfalls

### Pitfall 1: TSan and ASan cannot run simultaneously
**What goes wrong:** Trying to build with `-fsanitize=address,thread` fails -- they are mutually exclusive.
**Why it happens:** Both sanitizers use incompatible runtime instrumentation.
**How to avoid:** Two separate builds: `build-asan` with `address,undefined` and `build-tsan` with `thread`.
**Warning signs:** Link errors mentioning conflicting sanitizer runtimes.

### Pitfall 2: TSan false positives from signal handlers
**What goes wrong:** TSan may report races in signal handlers pushing to the lock-free EventQueue.
**Why it happens:** TSan doesn't fully understand lock-free patterns that use explicit memory ordering.
**How to avoid:** The EventQueue uses acquire/release ordering correctly. If TSan reports false positives on the EventQueue, use `__attribute__((no_sanitize("thread")))` annotations or TSan suppression files. Check if the reports are actual data races or false positives from the SPSC pattern.
**Warning signs:** TSan reports pointing to EventQueue::push() or atomic operations in signal handlers.

### Pitfall 3: Tests linking against btop.cpp globals
**What goes wrong:** Tests that reference Global::app_state or other globals require linking the entire btop translation unit, pulling in massive dependencies.
**Why it happens:** btop.cpp defines the globals but also contains main loop, Runner, signal handlers, etc.
**How to avoid:** The project already handles this -- `libbtop_test` links against `libbtop` (the OBJECT library containing all src/*.cpp except main.cpp). Tests use local instances where possible. For globals that must exist (e.g., app_state is extern), the linker resolves them from libbtop.
**Warning signs:** Undefined reference errors for Global:: symbols.

### Pitfall 4: Runner FSM tests needing a running thread
**What goes wrong:** Trying to test Runner::_runner() directly requires standing up the entire btop environment.
**Why it happens:** _runner() depends on Config, Term, Draw, Proc, etc.
**How to avoid:** Test observable properties (RunnerStateTag, is_active, is_stopping, wait_idle) on local atomics. For integration-level testing, rely on sanitizer sweeps of the full binary.
**Warning signs:** Tests requiring Config::init() or Term::init() setup.

### Pitfall 5: macOS-specific sanitizer behavior
**What goes wrong:** macOS Clang's TSan or ASan may behave differently from Linux GCC/Clang.
**Why it happens:** Different runtime libraries, different interceptors.
**How to avoid:** Accept macOS as the primary platform (this is the dev environment). Run tests here. Note any platform-specific suppressions needed.
**Warning signs:** Sanitizer reports about system library functions (libsystem, dyld).

### Pitfall 6: Incomplete catch-all coverage creating false confidence
**What goes wrong:** Testing only the explicit on_event overloads and assuming the catch-all "just works" for all 40 remaining pairs.
**Why it happens:** Developer assumes the catch-all template is trivially correct.
**How to avoid:** Test at least one representative event for each non-Running state (Resizing, Reloading, Sleeping, Quitting, Error). The AllSixStatesRouteCorrectly test already does this with TimerTick, but adding Quit for non-terminal states and Resume for non-Sleeping states provides stronger coverage.
**Warning signs:** A catch-all change silently breaking state preservation.

## Code Examples

### Pattern: Complete state-event pair test
```cpp
// Source: Existing project pattern from tests/test_transitions.cpp

// Test a catch-all pair: non-Running state ignores non-matching event
TEST(AppFsmTransition, ResizingIgnoresReloadPreservesState) {
    AppStateVar current = state::Resizing{};
    auto next = dispatch_event(current, event::Reload{});
    EXPECT_TRUE(std::holds_alternative<state::Resizing>(next));
}

// Test the special Sleeping+Resume -> Resizing transition
TEST(AppFsmTransition, SleepingResumeTransitionsToResizing) {
    AppStateVar current = state::Sleeping{};
    auto next = dispatch_event(current, event::Resume{});
    EXPECT_TRUE(std::holds_alternative<state::Resizing>(next));
}

// Test catch-all preserves data (Quitting exit code preserved on any event)
TEST(AppFsmTransition, QuittingPreservesExitCodeOnQuitEvent) {
    AppStateVar current = state::Quitting{42};
    auto next = dispatch_event(current, event::Quit{99});
    ASSERT_TRUE(std::holds_alternative<state::Quitting>(next));
    EXPECT_EQ(std::get<state::Quitting>(next).exit_code, 42);  // Original preserved
}
```

### Pattern: Runner FSM observable state tests
```cpp
// Source: Project conventions from tests/test_runner_state.cpp

// Test is_active() query function
TEST(RunnerFsmQuery, IsActiveForCollecting) {
    Global::runner_state_tag.store(Global::RunnerStateTag::Collecting, std::memory_order_release);
    EXPECT_TRUE(Runner::is_active());
}

TEST(RunnerFsmQuery, IsActiveForDrawing) {
    Global::runner_state_tag.store(Global::RunnerStateTag::Drawing, std::memory_order_release);
    EXPECT_TRUE(Runner::is_active());
}

TEST(RunnerFsmQuery, NotActiveForIdle) {
    Global::runner_state_tag.store(Global::RunnerStateTag::Idle, std::memory_order_release);
    EXPECT_FALSE(Runner::is_active());
}

TEST(RunnerFsmQuery, NotActiveForStopping) {
    Global::runner_state_tag.store(Global::RunnerStateTag::Stopping, std::memory_order_release);
    EXPECT_FALSE(Runner::is_active());
}

TEST(RunnerFsmQuery, IsStoppingTrue) {
    Global::runner_state_tag.store(Global::RunnerStateTag::Stopping, std::memory_order_release);
    EXPECT_TRUE(Runner::is_stopping());
}

TEST(RunnerFsmQuery, IsStoppingFalseForCollecting) {
    Global::runner_state_tag.store(Global::RunnerStateTag::Collecting, std::memory_order_release);
    EXPECT_FALSE(Runner::is_stopping());
}
```

### Pattern: Sanitizer build and run
```bash
# ASan+UBSan sweep
cmake -B build-asan -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
    -DBTOP_SANITIZER=address,undefined
cmake --build build-asan -j$(sysctl -n hw.ncpu)
# Run unit tests under ASan
ctest --test-dir build-asan --output-on-failure
# Run the application briefly under ASan (manual exercise)
timeout 5 ./build-asan/btop || true

# TSan sweep
cmake -B build-tsan -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
    -DBTOP_SANITIZER=thread
cmake --build build-tsan -j$(sysctl -n hw.ncpu)
# Run unit tests under TSan
ctest --test-dir build-tsan --output-on-failure
# Run the application briefly under TSan (manual exercise)
timeout 5 ./build-tsan/btop || true
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| No FSM tests | Unit tests for state types + event types + some transitions | Phases 10-14 | Partial coverage established |
| No sanitizer in CI | CMake BTOP_SANITIZER variable | Phase 6 (v1.0) | Build system ready |
| Manual bool flag testing | std::variant + std::visit dispatch testing | Phase 12-13 | Enables exhaustive matrix testing |

**Current state:** ~50% of App FSM pairs tested, 0% of Runner FSM transitions tested, sanitizer builds exist but not run with tests.

## Open Questions

1. **Runner FSM transition integration testing**
   - What we know: _runner() is tightly coupled to the rest of btop. Cannot unit-test the thread function directly.
   - What's unclear: Whether testing is_active()/is_stopping()/wait_idle() on the atomic is sufficient for VERIFY-02, or if deeper thread-level testing is needed.
   - Recommendation: Test the observable state queries (is_active, is_stopping, wait_idle, to_runner_tag) thoroughly. The sanitizer sweeps (VERIFY-03/04) cover thread-level behavior of the actual runner thread. This two-pronged approach satisfies the requirement.

2. **VERIFY-05: Behavioral preservation validation**
   - What we know: No automated end-to-end behavioral tests exist. The project has always relied on manual testing.
   - What's unclear: What constitutes "proof" that behavior is unchanged.
   - Recommendation: Rely on (a) all existing unit tests passing, (b) sanitizer sweeps showing zero regressions, and (c) a documented manual smoke test checklist (keyboard shortcuts, resize, suspend/resume, quit, config reload). Mark VERIFY-05 as partially manual.

3. **TSan suppression file needed?**
   - What we know: Lock-free SPSC queue is correct but TSan may not understand the pattern.
   - What's unclear: Whether Apple Clang TSan 17.0 handles this correctly.
   - Recommendation: Run TSan sweep first. Only create suppression file if false positives appear. Document any suppressions.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 (via CMake FetchContent) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `ctest --test-dir build-test --output-on-failure -R "AppFsm\|RunnerFsm\|TypedTransition"` |
| Full suite command | `ctest --test-dir build-test --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| VERIFY-01 | App FSM (state,event) pair coverage | unit | `ctest --test-dir build-test -R "TypedTransition\|TypedDispatch\|AppFsmTransition" --output-on-failure` | Partial -- test_transitions.cpp exists, needs extension |
| VERIFY-02 | Runner FSM transition coverage | unit | `ctest --test-dir build-test -R "RunnerFsm" --output-on-failure` | No -- test_runner_state.cpp has struct tests but no transition tests |
| VERIFY-03 | ASan/UBSan zero findings | integration | `ctest --test-dir build-asan --output-on-failure` | No -- build-asan needs BUILD_TESTING=ON |
| VERIFY-04 | TSan zero findings | integration | `ctest --test-dir build-tsan --output-on-failure` | No -- build-tsan needs BUILD_TESTING=ON |
| VERIFY-05 | Behavioral preservation | manual + unit | All existing tests pass; manual smoke test | Existing tests exist; smoke test checklist new |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build-test --output-on-failure`
- **Per wave merge:** Full suite + ASan + TSan builds
- **Phase gate:** All three builds (test, asan, tsan) green; zero sanitizer findings

### Wave 0 Gaps
- [ ] `tests/test_transitions.cpp` needs extension -- missing ~28 App FSM pairs (VERIFY-01)
- [ ] `tests/test_runner_state.cpp` needs Runner FSM transition/query tests (VERIFY-02)
- [ ] build-asan needs rebuild with `BUILD_TESTING=ON` (VERIFY-03)
- [ ] build-tsan needs rebuild with `BUILD_TESTING=ON` (VERIFY-04)

## Sources

### Primary (HIGH confidence)
- Project source: `src/btop_state.hpp` -- AppStateTag, RunnerStateTag, state/runner structs, variant types
- Project source: `src/btop_events.hpp` -- AppEvent variant, EventQueue template, dispatch_event declaration
- Project source: `src/btop.cpp` lines 299-354 -- on_event overloads and dispatch_event implementation
- Project source: `src/btop.cpp` lines 517-832 -- Runner::_runner() thread function
- Project source: `src/btop.cpp` lines 977-1065 -- on_exit, on_enter, transition_to
- Project source: `src/btop_shared.hpp` lines 411-443 -- Runner::is_active, is_stopping, wait_idle, run, stop
- Project source: `tests/CMakeLists.txt` -- GoogleTest v1.17.0, test executable configuration
- Project source: `tests/test_transitions.cpp` -- existing App FSM transition tests
- Project source: `tests/test_runner_state.cpp` -- existing Runner FSM struct/variant tests
- Project source: `tests/test_events.cpp` -- existing event type + EventQueue tests
- Project source: `tests/test_app_state.cpp` -- existing AppStateTag + state struct tests
- Project source: `CMakeLists.txt` lines 51-124 -- BTOP_SANITIZER configuration

### Secondary (MEDIUM confidence)
- Build artifacts: `build-asan/CMakeCache.txt` -- confirms BTOP_SANITIZER=address,undefined, BUILD_TESTING=OFF
- Build artifacts: `build-tsan/CMakeCache.txt` -- confirms BTOP_SANITIZER=thread, BUILD_TESTING=OFF
- Compiler: Apple Clang 17.0.0 supports ASan, UBSan, TSan on arm64-darwin

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all tools already in project, no new dependencies needed
- Architecture: HIGH -- complete FSM implementation read and state-event matrix enumerated from source
- Pitfalls: HIGH -- based on direct analysis of project structure and sanitizer compatibility
- Test gaps: HIGH -- compared existing test coverage against enumerated (state, event) matrix

**Research date:** 2026-03-01
**Valid until:** 2026-03-31 (stable -- verification of completed code, no moving targets)
