---
phase: 16-runner-error-path-purity
verified: 2026-03-01T22:00:00Z
status: passed
score: 8/8 must-haves verified
re_verification: false
---

# Phase 16: Runner Error Path Purity Verification Report

**Phase Goal:** Runner thread errors flow through the event-driven architecture instead of bypassing it with direct shadow atomic writes
**Verified:** 2026-03-01T22:00:00Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth | Status | Evidence |
|----|-------|--------|----------|
| 1  | Runner thread exception handler pushes event::ThreadError into event queue instead of directly writing Global::app_state | VERIFIED | `btop.cpp:715` — `Global::event_queue.push(event::ThreadError{})` in catch block; zero matches for `app_state.store(AppStateTag::Error)` anywhere in btop.cpp |
| 2  | Runner thread safety-check error pushes event::ThreadError into event queue instead of directly writing Global::app_state | VERIFIED | `btop.cpp:539` — `Global::event_queue.push(event::ThreadError{})` in safety check block |
| 3  | Main loop receives ThreadError event and transitions app_var to state::Error via dispatch_event + transition_to | VERIFIED | `btop.cpp:333-335` — `on_event(Running, ThreadError)` returns `state::Error{std::move(Global::exit_error_msg)}`; `transition_to` at line 1054 stores `app_state.store(to_tag(next))` |
| 4  | on_event(Running, ThreadError) is reachable at runtime (not dead code) and produces state::Error with the actual error message | VERIFIED | Test `TypedTransition.RunningThreadErrorReturnsError` passes: sets `Global::exit_error_msg = "Test runner error"`, verifies returned state contains that message and that `exit_error_msg` is consumed (moved out). Test passes in normal, ASan, and TSan builds |
| 5  | runner_var local variable in _runner() is removed (no write-only dead code) | VERIFIED | `grep -n "runner_var" src/btop.cpp` returns zero matches |
| 6  | RunnerStateVar typedef, runner:: namespace structs, and to_runner_tag() are removed (no unused type scaffolding) | VERIFIED | `grep` across all of `src/` and `tests/` for `RunnerStateVar`, `to_runner_tag`, `runner::Idle`, `runner::Collecting`, `runner::Drawing`, `runner::Stopping` returns zero matches; `btop_state.hpp` contains only `RunnerStateTag` enum and `runner_state_tag` atomic (retained, actively used) |
| 7  | When runner reports an error, both app_var (variant) and Global::app_state (shadow atomic) hold Error state | VERIFIED | `on_event(Running, ThreadError)` returns `state::Error`; `transition_to()` at line 1054 calls `Global::app_state.store(to_tag(next))` — these are linked. `ThreadErrorSyncsVariantAndShadow` test confirms dispatch produces `state::Error`; `ShadowEnumUpdatedOnStateTypeChange` test (EntryExit group) confirms `transition_to` syncs shadow atomic |
| 8  | All changes pass ASan+UBSan and TSan sweeps with zero findings | VERIFIED | Both `build-asan` and `build-tsan` test binaries exist and pass all 30 ThreadError/Runner-related tests with zero sanitizer findings |

**Score:** 8/8 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/btop.cpp` | Event-driven error path in _runner(), updated on_event(Running, ThreadError) | VERIFIED | Lines 539 and 715: two `event_queue.push(event::ThreadError{})` calls replacing former direct `app_state.store(Error)` calls. Line 334: `on_event` returns `state::Error{std::move(Global::exit_error_msg)}`. Line 981: `on_exit(Running, Error)` stops runner thread. |
| `src/btop_state.hpp` | Cleaned state definitions without runner variant types | VERIFIED | File is 114 lines. Contains only: `AppStateTag` enum, `app_state` atomic, `RunnerStateTag` enum, `runner_state_tag` atomic, `state::*` structs, `AppStateVar`, `to_tag()`. No `RunnerStateVar`, no `runner::` namespace, no `to_runner_tag()`. |
| `tests/test_transitions.cpp` | Updated ThreadError dispatch tests with real error message verification | VERIFIED | Line 50-58: `RunningThreadErrorReturnsError` sets sideband, checks message propagation and that `exit_error_msg` is consumed. Lines 333-348: `ThreadErrorSyncsVariantAndShadow` integration test added. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/btop.cpp` catch block (line 713-717) | `Global::event_queue` | `event_queue.push(event::ThreadError{})` | WIRED | Pattern `event_queue\.push\(event::ThreadError` found at lines 539 and 715 |
| `src/btop.cpp` on_event(Running, ThreadError) (line 333) | `state::Error` | Returns `state::Error{std::move(Global::exit_error_msg)}` | WIRED | Line 334 returns state::Error with moved exit_error_msg |
| `src/btop.cpp` transition_to (line 1054) | `Global::app_state` shadow atomic | `app_state.store(to_tag(next))` | WIRED | Line 1054: `Global::app_state.store(to_tag(next), std::memory_order_release)` |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| PURE-01 | 16-01-PLAN.md | Runner exception handler pushes event::ThreadError into event queue instead of writing shadow atomic directly | SATISFIED | `btop.cpp:715` pushes ThreadError; zero `app_state.store(AppStateTag::Error)` calls remain in btop.cpp |
| PURE-02 | 16-01-PLAN.md | on_event(Running, ThreadError) is reachable and tested (no dead code) | SATISFIED | Runner now pushes events, making the handler reachable. Test `TypedTransition.RunningThreadErrorReturnsError` verifies runtime behavior |
| PURE-03 | 16-01-PLAN.md | RunnerStateVar is either used for runtime dispatch or removed (no write-only dead code) | SATISFIED | `RunnerStateVar`, `runner::` structs, `to_runner_tag()`, and `runner_var` local variable all removed; confirmed by exhaustive grep across src/ and tests/ |
| PURE-04 | 16-01-PLAN.md | When runner reports error, app_var transitions to state::Error (variant/shadow in sync) | SATISFIED | Error path: push event -> dispatch_event -> on_event returns state::Error -> transition_to syncs shadow. `ThreadErrorSyncsVariantAndShadow` and `ShadowEnumUpdatedOnStateTypeChange` tests cover both halves |

All four requirements are satisfied. REQUIREMENTS.md shows all four marked `[x]` and as "Complete" in the status table.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/btop.cpp` | 918, 935 | `exit_error_msg` set + `clean_quit(1)` directly (no event push) | INFO | These are in `Runner::stop()` which runs on the main thread, not the runner thread. The plan explicitly excludes these: "Do NOT touch the Runner::stop() function error paths" — they are acceptable main-thread error paths, not shadow atomic bypasses. |

No blocker or warning-level anti-patterns found. The single INFO item is intentional and plan-sanctioned.

### Pre-Existing Test Failure (Out of Scope)

`RingBuffer.PushBackOnZeroCapacity` fails in all build configurations. This failure originates from commit `9affeac` (phase 04), predates phase 16 by a large margin, and was explicitly noted as out of scope in the SUMMARY. It does not affect any phase 16 code paths.

### Human Verification Required

None. All goal truths are verifiable through grep and test execution. The sanitizer sweeps (ASan+UBSan and TSan) cover thread-safety concerns that would normally require human analysis.

### Commits Verified

| Commit | Description | Exists |
|--------|-------------|--------|
| `20e2ddd` | feat(16-01): route runner error paths through event queue | YES |
| `381353a` | refactor(16-01): remove runner_var dead code and unused type scaffolding | YES |
| `a29e002` | test(16-01): verify error path purity with updated tests and sanitizer sweeps | YES |

### Gaps Summary

None. All eight must-have truths are verified by direct code inspection and test execution. The phase goal is fully achieved: runner thread error paths no longer bypass the event-driven FSM — they push `event::ThreadError` into the event queue, which the main loop pops and dispatches through `on_event(Running, ThreadError)` returning `state::Error`, with `transition_to()` syncing both the authoritative `app_var` variant and the `Global::app_state` shadow atomic.

---

_Verified: 2026-03-01T22:00:00Z_
_Verifier: Claude (gsd-verifier)_
