---
status: diagnosed
trigger: "Investigate why btop hangs when pressing Ctrl+C instead of exiting cleanly"
created: 2026-02-28T00:00:00Z
updated: 2026-02-28T00:00:00Z
---

## Current Focus

hypothesis: clean_quit() early-returns because shadow atomic is already Quitting, causing infinite main loop
test: trace the exact sequence of state changes through the quit path
expecting: confirm that clean_quit guards prevent execution when called via on_enter(Quitting)
next_action: report root cause

## Symptoms

expected: Ctrl+C causes clean shutdown via signal -> event::Quit -> dispatch -> transition -> on_enter(Quitting) -> clean_quit() -> _Exit()
actual: btop hangs after Ctrl+C, never exits
errors: none visible (hangs silently)
reproduction: press Ctrl+C while btop is running
started: after Phase 13 variant-based state machine refactor

## Eliminated

- hypothesis: Signal handler not pushing Quit event
  evidence: _signal_handler (line 351) correctly pushes event::Quit{0} on SIGINT and calls Input::interrupt()
  timestamp: 2026-02-28

- hypothesis: Main loop not draining queue or not dispatching
  evidence: Main loop (line 1445) drains with try_pop() and calls dispatch_event + transition_to correctly
  timestamp: 2026-02-28

- hypothesis: dispatch_event routing is wrong
  evidence: on_event(Running, Quit) at line 298 correctly returns state::Quitting{e.exit_code}
  timestamp: 2026-02-28

- hypothesis: transition_to not firing on_exit/on_enter
  evidence: transition_to (line 997) correctly fires on_exit then on_enter when tags differ; Running->Quitting tags differ so both fire
  timestamp: 2026-02-28

## Evidence

- timestamp: 2026-02-28
  checked: signal handler (_signal_handler, line 351-375)
  found: Correctly pushes event::Quit{0} for SIGINT, calls Input::interrupt() to wake pselect
  implication: Signal path is correct

- timestamp: 2026-02-28
  checked: on_event(Running, Quit) at line 298
  found: Returns state::Quitting{e.exit_code} -- correct
  implication: Event dispatch is correct

- timestamp: 2026-02-28
  checked: transition_to() at line 997-1019
  found: (1) Calls on_exit(Running, Quitting) -> sets Runner::stopping=true. (2) Stores AppStateTag::Quitting to shadow atomic. (3) Calls on_enter(Quitting, ctx) -> calls clean_quit(q.exit_code).
  implication: transition_to correctly sequences exit->shadow->enter

- timestamp: 2026-02-28
  checked: clean_quit() at line 215-266
  found: LINE 216 GUARD: `if (Global::app_state.load() == AppStateTag::Quitting) return;` But transition_to ALREADY stored AppStateTag::Quitting at line 1010 BEFORE calling on_enter at line 1017.
  implication: ROOT CAUSE FOUND -- clean_quit() sees Quitting in the atomic and returns immediately without doing any cleanup or calling _Exit()

- timestamp: 2026-02-28
  checked: Main loop termination (lines 1442-1523)
  found: The main loop is `while (not true not_eq not false)` which is `while(true)` -- an infinite loop. After the event drain breaks on Quitting (line 1453), control falls to section 2 (state chains). Quitting is not handled there. Then section 3 (term_resize), section 4 (clock), section 5 (timer tick) -- running is nullptr so those skip. Section 6 (input polling) -- running is nullptr so it skips. Loop iterates back to top, event queue is now empty, so drain does nothing. app_var is still Quitting but nothing ever exits.
  implication: The main loop spins forever in state::Quitting because clean_quit() returned early and nothing else breaks the infinite loop

## Resolution

root_cause: Two cooperating bugs cause the hang:

  1. **transition_to() stores AppStateTag::Quitting to the shadow atomic (line 1010) BEFORE calling on_enter(Quitting) (line 1017).** When on_enter(Quitting) calls clean_quit(), the guard at line 216 (`if (Global::app_state.load() == AppStateTag::Quitting) return;`) sees the already-stored Quitting tag and returns immediately without performing cleanup or calling _Exit().

  2. **The main loop (line 1443) is `while (not true not_eq not false)` which evaluates to `while(true)` -- an unconditional infinite loop.** After the event drain inner loop breaks on Quitting/Error (line 1453), control falls through to the state chain handlers (sections 2-6), none of which handle Quitting. The loop iterates forever with app_var stuck in state::Quitting, the event queue empty, and no exit path.

  The pre-Phase-13 code presumably had the main loop conditioned on `while (app_state != Quitting)` so even if clean_quit failed, the loop would exit and btop_main would return. The Phase 13 refactor changed the loop to infinite and relied on clean_quit() calling _Exit() -- but the shadow atomic guard in clean_quit() defeats that.

fix: (not applied -- diagnosis only)
verification: (not applied -- diagnosis only)
files_changed: []
