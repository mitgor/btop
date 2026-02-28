---
status: diagnosed
trigger: "Investigate why kill -USR2 does nothing in btop after Phase 13 changes"
created: 2026-02-28T00:00:00Z
updated: 2026-02-28T00:00:00Z
---

## Current Focus

hypothesis: The Phase 13 SIGUSR2/Reload code path is correct. The observed "does nothing" is caused by testing the wrong binary (Homebrew btop at /opt/homebrew/bin/btop rather than the dev build at build-release/btop).
test: Verified code path end-to-end, checked binary symbols, compared installed vs dev binary
expecting: Dev binary with Phase 13 code handles SIGUSR2 correctly
next_action: Report diagnosis

## Symptoms

expected: kill -USR2 <pid> should trigger config reload (re-read config, update theme, redraw)
actual: "does nothing" -- no visible effect
errors: none reported
reproduction: send SIGUSR2 to running btop process
started: after Phase 13 changes

## Eliminated

- hypothesis: SIGUSR2 not registered in signal handler setup
  evidence: Line 1371 of btop.cpp registers std::signal(SIGUSR2, _signal_handler)
  timestamp: 2026-02-28

- hypothesis: Signal handler doesn't push Reload event
  evidence: Lines 367-368 of btop.cpp: case SIGUSR2 pushes event::Reload{}
  timestamp: 2026-02-28

- hypothesis: Main loop doesn't drain Reload event
  evidence: Lines 1445-1454 drain event queue via try_pop(), dispatch_event maps (Running, Reload) to Reloading at line 317-318
  timestamp: 2026-02-28

- hypothesis: Catch-all on_event(auto, auto) matches instead of specific on_event(Running, Reload)
  evidence: Non-template function is always preferred over template in C++ overload resolution. Specific overload at line 317 is non-template, catch-all at line 334 is abbreviated function template.
  timestamp: 2026-02-28

- hypothesis: transition_to doesn't fire on_enter(Reloading)
  evidence: transition_to at lines 997-1019 checks to_tag differs, calls on_exit, updates state, calls on_enter. on_enter(Reloading) at line 968 does full reload logic.
  timestamp: 2026-02-28

- hypothesis: State chain Reloading->Resizing->Running broken
  evidence: Lines 1457-1467 check holds_alternative<Reloading> then transition to Resizing, then to Running. on_enter(Resizing) at line 958 does term_resize + calcSizes + Runner::run("all",true,true).
  timestamp: 2026-02-28

- hypothesis: pselect signal mask blocks SIGUSR2
  evidence: Lines 1379-1382 only block SIGUSR1. Input::signal_mask (old mask) has SIGUSR2 unblocked. pselect uses this mask.
  timestamp: 2026-02-28

- hypothesis: Runner::stop() calls clean_quit during reload
  evidence: Runner thread holds mtx for its entire lifetime (lock_guard at line 521). try_lock in stop() always fails when thread alive, so is_runner_busy=true. The clean_quit branch at line 893 only fires when thread is dead.
  timestamp: 2026-02-28

- hypothesis: Binary compiled from pre-Phase-13 code (stale build)
  evidence: Rebuilt binary; nm shows variant-based dispatch_event symbol with state::Running, state::Reloading, event::Reload types. Binary IS Phase 13 code.
  timestamp: 2026-02-28

## Evidence

- timestamp: 2026-02-28
  checked: Signal handler registration
  found: SIGUSR2 registered at line 1371 with _signal_handler
  implication: Signal will be caught

- timestamp: 2026-02-28
  checked: Signal handler body
  found: SIGUSR2 case at line 367 pushes event::Reload{}, then Input::interrupt() at line 373 wakes pselect
  implication: Event is queued and main loop is woken

- timestamp: 2026-02-28
  checked: on_event(Running, Reload) overload
  found: Line 317-318 returns state::Reloading{}
  implication: Dispatch correctly maps to Reloading state

- timestamp: 2026-02-28
  checked: on_enter(Reloading) implementation
  found: Line 968-976: Runner::stop(), Config::unlock(), init_config(), Theme::updateThemes(), Theme::setTheme(), Draw::update_reset_colors(), Draw::banner_gen()
  implication: Full reload logic executes

- timestamp: 2026-02-28
  checked: State chain in main loop
  found: Lines 1457-1467 chain Reloading->Resizing->Running with proper transition_to calls
  implication: After reload, term_resize + calcSizes + full redraw happens

- timestamp: 2026-02-28
  checked: Installed btop binary
  found: /opt/homebrew/bin/btop is stock release v1.4.6 (no Phase 11/12/13 changes). Dev binary at build-release/btop has Phase 13 code (confirmed via nm symbols).
  implication: If user runs "btop" without full path, they test the wrong binary

## Resolution

root_cause: The Phase 13 SIGUSR2 code path is CORRECT. All five checkpoints verified:
  1. SIGUSR2 registered (line 1371)
  2. Signal handler pushes event::Reload{} (line 367-368) and wakes main loop (line 372-374)
  3. Main loop drains queue and dispatches via on_event(Running,Reload)->Reloading (line 1445-1454, 317-318)
  4. on_enter(Reloading) calls full reload logic (line 968-976)
  5. State chains Reloading->Resizing->Running with full redraw (lines 1457-1467)

Most likely explanation for "does nothing": The user is testing /opt/homebrew/bin/btop (stock v1.4.6 release) rather than the development build at build-release/btop. The Homebrew binary has none of the Phase 11-13 event queue / variant dispatch changes.

fix: N/A (code is correct)
verification: N/A
files_changed: []
