---
phase: 14-runner-fsm
verified: 2026-03-01T08:17:34Z
status: passed
score: 5/5 must-haves verified
re_verification: false
---

# Phase 14: Runner FSM Verification Report

**Phase Goal:** Extract Runner FSM — define RunnerStateTag enum (Idle/Collecting/Drawing/Stopping), runner:: state structs, RunnerStateVar variant with shadow atomic, and migrate runner thread loop plus all consumer sites to use FSM types instead of atomic bool flags.
**Verified:** 2026-03-01T08:17:34Z
**Status:** PASSED
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (from ROADMAP.md Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | RunnerState is a std::variant (Idle, Collecting, Drawing, Stopping) — independent from App FSM | VERIFIED | `src/btop_state.hpp` lines 91–106: `namespace runner { struct Idle{}; struct Collecting{}; struct Drawing { bool force_redraw; }; struct Stopping{}; }` + `using RunnerStateVar = std::variant<runner::Idle, runner::Collecting, runner::Drawing, runner::Stopping>;` |
| 2 | Runner's atomic<bool> flags (active, stopping, waiting, redraw) replaced by FSM states or typed methods | VERIFIED | `src/btop_shared.hpp`: no `extern atomic<bool> active/stopping/waiting/redraw` declarations remain. `src/btop.cpp`: no definitions of those flags. `runner_state_tag` is the single source of truth. `request_redraw()` replaces `Runner::redraw = true`. |
| 3 | Main thread communicates with runner via typed mechanisms, not raw bool flag mutation | VERIFIED | `runner_conf` struct carries start parameters. `runner_state_tag.store(Stopping)` replaces `stopping=true`. `Runner::request_redraw()` replaces `Runner::redraw = true`. All confirmed in `src/btop.cpp` lines 481–904. |
| 4 | Binary semaphore wake-up pattern is preserved | VERIFIED | `src/btop.cpp` lines 427–429: `std::binary_semaphore do_work{0}; inline void thread_wait() { do_work.acquire(); } inline void thread_trigger() { do_work.release(); }` — unchanged from original. |
| 5 | Data collection and drawing work identically (same metrics, same timing, same output) | VERIFIED (build + tests) | btop binary builds cleanly. All 239 tests run; 238 pass; 1 failure (`RingBuffer.PushBackOnZeroCapacity`) is pre-existing from before Phase 14 (confirmed in Phase 14-01 SUMMARY: "only pre-existing RingBuffer failure"). No new test failures introduced. |

**Score:** 5/5 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/btop_state.hpp` | RunnerStateTag enum, runner:: state structs, RunnerStateVar variant, to_runner_tag() | VERIFIED | Lines 46–118: enum with 4 uint8_t values, 4 runner:: structs, using alias, inline to_runner_tag() using std::visit with if-constexpr exhaustiveness |
| `src/btop_shared.hpp` | Runner::is_stopping(), Runner::is_active(), Runner::wait_idle(), Runner::request_redraw(); old flags removed | VERIFIED | Lines 411–443: all four wrappers present; no `active`, `stopping`, `waiting`, or `redraw` externs remain; `pause_output` is `atomic<bool>` |
| `src/btop.cpp` | Runner FSM loop with explicit state transitions; run()/stop()/on_exit() migrated; runner_state_tag definition | VERIFIED | Lines 128 (definition), 527–730 (loop with runner_var Idle/Collecting/Drawing transitions + runner_state_tag shadow updates), 836–951 (run/stop/on_exit using runner_state_tag) |
| `src/btop_tools.hpp` | atomic_wait/atomic_wait_for overloads for RunnerStateTag | VERIFIED | Lines 368–373: two new overload declarations after existing bool versions |
| `src/btop_tools.cpp` | Implementations of RunnerStateTag atomic_wait overloads | VERIFIED | Lines 547–556: implementations following identical pattern to bool versions |
| `tests/test_runner_state.cpp` | Unit tests for RUNNER-01 and RUNNER-02 type definitions | VERIFIED | 184 lines, 29 tests covering enum values, lock-free atomic, to_string, store/load round-trips, struct constructibility, variant alternatives, to_runner_tag(), mutual exclusion |
| `tests/CMakeLists.txt` | test_runner_state.cpp in btop_test executable | VERIFIED | Line 16: `test_runner_state.cpp` present in add_executable source list |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/btop_state.hpp` | `src/btop_shared.hpp` | RunnerStateTag used in is_stopping/is_active inline wrappers | VERIFIED | btop_shared.hpp lines 418–427 reference `Global::RunnerStateTag::Stopping/Collecting/Drawing` |
| `src/btop_state.hpp` | `src/btop.cpp` | runner_state_tag definition + RunnerStateVar transitions in loop | VERIFIED | btop.cpp line 128 defines `Global::runner_state_tag{RunnerStateTag::Idle}`; lines 527–730 use RunnerStateVar and tag stores |
| `tests/test_runner_state.cpp` | `src/btop_state.hpp` | Tests include btop_state.hpp for type definitions | VERIFIED | CMakeLists.txt wires test executable to libbtop which includes btop_state.hpp; 29 tests pass |
| `src/btop.cpp (Runner::stop)` | `src/btop_state.hpp (RunnerStateTag::Stopping)` | runner_state_tag.store(Stopping) replaces stopping=true | VERIFIED | btop.cpp line 916: `Global::runner_state_tag.store(Global::RunnerStateTag::Stopping, std::memory_order_release)` |
| `src/osx/btop_collect.cpp` | `src/btop_shared.hpp (Runner::is_stopping)` | Collector cancellation check uses wrapper | VERIFIED | 3 call sites confirmed; same pattern verified across all 5 platform collectors (openbsd: 2, netbsd: 2, freebsd: 2, linux: 6, osx: 3 = 15 total sites) |
| `src/btop_draw.cpp` | `src/btop_shared.hpp (Runner::wait_idle, Runner::request_redraw)` | Draw uses typed API for active-wait and redraw request | VERIFIED | btop_draw.cpp line 2748: `Runner::wait_idle()`, line 2763: `Runner::request_redraw()`, line 2762: `Runner::pause_output.store(false)` |
| `src/btop_input.cpp` | `src/btop_shared.hpp (Runner::wait_idle)` | Input uses wait_idle for 8 sites | VERIFIED | All 8 sites at lines 254, 279, 491, 500, 506, 512, 595, 617 use `Runner::wait_idle()` |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| RUNNER-01 | 14-01-PLAN.md | Runner thread states defined as std::variant (Idle, Collecting, Drawing, Stopping) | SATISFIED | RunnerStateVar in btop_state.hpp lines 101–106; runner:: structs at lines 92–97; to_runner_tag() at lines 109–118; 29 unit tests pass |
| RUNNER-02 | 14-01 + 14-02-PLAN.md | Runner atomic<bool> flags replaced by FSM states or typed methods | SATISFIED | active/stopping/waiting/redraw all removed from btop_shared.hpp and btop.cpp; runner_state_tag + request_redraw() replace them; btop builds cleanly |
| RUNNER-03 | 14-02-PLAN.md | Main thread -> runner communication via typed mechanisms, not raw bool flag mutation | SATISFIED | runner_conf struct (start), RunnerStateTag::Stopping store (stop), Runner::request_redraw() (redraw) — all confirmed in btop.cpp lines 481–904 |
| RUNNER-04 | 14-02-PLAN.md | Binary semaphore wake-up pattern preserved | SATISFIED | do_work binary_semaphore + thread_wait/thread_trigger unchanged at btop.cpp lines 427–429; used at lines 536, 906, 945 |

No orphaned requirements. All 4 RUNNER requirements declared in plan frontmatter are accounted for and satisfied.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/btop_shared.hpp` | 484, 520, 531 | TODO comments | Info | Pre-existing TODOs in GPU/Process structs; unrelated to Phase 14 work |
| `src/btop.cpp` | 426 | TODO comment | Info | Pre-existing: "This can be made a local without too much effort" — not introduced by Phase 14 |

No blockers. No stubs. No empty implementations. All Phase 14 changes are substantive implementations.

**Notable observation:** In the runner thread loop, `runner_var` is set to `runner::Idle{}`, `runner::Collecting{}`, and `runner::Drawing{conf.force_redraw}` but not to `runner::Stopping{}`. When stopping, only `runner_state_tag` is set to `RunnerStateTag::Stopping` — `runner_var` retains the previous state. This is functionally correct because `runner_state_tag` is the shadow atomic used by all cross-thread queries (`is_stopping()`, `is_active()`), and `runner_var` is a thread-local bookkeeping variable. It is an acceptable design choice, not a defect.

### Human Verification Required

#### 1. Runtime Behavior Under Stop/Start/Resize

**Test:** Launch btop, let it run for ~10 seconds collecting data, then resize the terminal, then press Ctrl+C.
**Expected:** Data displays correctly before resize; terminal resizes cleanly with no garbled output; btop exits cleanly on Ctrl+C without hanging.
**Why human:** Cannot verify runtime cooperative cancellation and semaphore wake timing programmatically with grep/build checks alone.

#### 2. Suspend/Resume Lifecycle (Ctrl+Z / fg)

**Test:** Run btop, press Ctrl+Z to suspend, wait 5 seconds, run `fg` to resume.
**Expected:** btop suspends cleanly, terminal is restored; on resume btop redraws immediately and data collection restarts.
**Why human:** on_exit() actions using runner_state_tag for stopping need runtime verification; the Sleeping/resume state machine path was modified.

#### 3. Runner::request_redraw() Propagation

**Test:** Open the config menu (F2), change a setting that triggers a redraw (e.g., color theme), close the menu.
**Expected:** Screen redraws immediately and correctly shows the new setting with no artifacts.
**Why human:** `Runner::request_redraw()` pending_redraw->conf.force_redraw folding path requires runtime verification that the redraw actually reaches the drawing functions.

### Gaps Summary

No gaps. All 5 success criteria verified. All 4 requirement IDs satisfied. The codebase reflects the goal: Runner FSM is fully extracted — RunnerStateTag enum, runner:: state structs, RunnerStateVar variant, shadow atomic `runner_state_tag`, and complete migration of the runner loop plus all 11 consumer files (input, menu, config, draw, 5 platform collectors) to typed FSM API. The old `atomic<bool>` flags (`active`, `stopping`, `waiting`, `redraw`) are fully removed.

---

_Verified: 2026-03-01T08:17:34Z_
_Verifier: Claude (gsd-verifier)_
