---
status: diagnosed
trigger: "Investigate why btop's static elements don't redraw after Ctrl+Z suspend and fg resume"
created: 2026-02-28T00:00:00Z
updated: 2026-02-28T00:00:00Z
---

## Current Focus

hypothesis: CONFIRMED -- Two interacting bugs cause resume to skip full redraw
test: Full code trace of Ctrl+Z -> fg flow through variant-based state machine
expecting: Missing Draw::calcSizes() and Runner::run("all", true, true)
next_action: Report root cause

## Symptoms

expected: After Ctrl+Z (suspend) and fg (resume), btop should fully redraw all static elements
actual: Static elements are missing; only a terminal resize forces them to redraw
errors: None (visual-only)
reproduction: Run btop, press Ctrl+Z, type fg, observe missing static elements
started: After Phase 13 refactored state machine from enum to variant dispatch

## Eliminated

- hypothesis: _resume() is never called
  evidence: Line 1447-1448 explicitly checks for event::Resume and calls _resume() before dispatch
  timestamp: 2026-02-28

- hypothesis: SIGCONT signal handler doesn't push Resume event
  evidence: Line 359-360 clearly pushes event::Resume{} on SIGCONT
  timestamp: 2026-02-28

- hypothesis: Section 3 (forced resize check) catches the resume and triggers redraw
  evidence: Section 2's Sleeping->Running transition calls transition_to() which overwrites Global::app_state from Resizing to Running at line 1010, BEFORE section 3 runs. Section 3 then checks Global::app_state == Resizing which is now false.
  timestamp: 2026-02-28

## Evidence

- timestamp: 2026-02-28
  checked: Full Ctrl+Z -> fg flow through state machine
  found: |
    1. Ctrl+Z: SIGTSTP -> event::Sleep{} pushed to queue
    2. Main loop drains Sleep: on_event(Running, Sleep) -> Sleeping{}
    3. transition_to(Running -> Sleeping): on_exit(Running, Sleeping) stops Runner, on_enter(Sleeping) calls _sleep()
    4. _sleep() calls Runner::stop(), Term::restore(), std::raise(SIGSTOP) -- PROCESS FROZEN HERE
    5. fg: SIGCONT delivered, signal handler pushes event::Resume{}, execution resumes after raise(SIGSTOP)
    6. _sleep() returns, on_enter(Sleeping) returns, transition_to returns. app_var = Sleeping{}
    7. Drain loop picks up Resume event:
       a. _resume() called: Term::init(), term_resize(true) sets Global::app_state = Resizing
       b. dispatch_event(Sleeping{}, Resume{}) -> catch-all returns Sleeping{} (NO on_event(Sleeping,Resume) handler!)
       c. transition_to(Sleeping -> Sleeping): same tag, SHORT-CIRCUITS. No entry/exit. Global::app_state stays Resizing
    8. Section 2: app_var is Sleeping -> transition_to(Sleeping -> Running)
       a. on_exit(Sleeping, Running): generic no-op
       b. Global::app_state = Running (line 1010) -- OVERWRITES Resizing flag!
       c. on_enter(Running): EMPTY BODY (line 991-993)
    9. Section 3: term_resize() without force returns early (no size change).
       Global::app_state == Resizing? NO -- it's Running. Section 3 SKIPPED.
    10. Section 5: Timer tick eventually fires Runner::run("all") WITHOUT force_redraw=true.
        Only diff-based rendering runs. Static elements not redrawn.
  implication: Two problems conspire: (a) no on_event(Sleeping, Resume) handler, and (b) on_enter(Running) has no redraw logic. The Sleeping->Running transition at section 2 overwrites the Resizing flag before section 3 can use it.

- timestamp: 2026-02-28
  checked: on_enter(state::Resizing&) at line 958-966
  found: Contains the full redraw logic: term_resize(true), Draw::calcSizes(), Runner::screen_buffer.resize(), Draw::update_clock(true), Runner::run("all", true, true)
  implication: This is the code that SHOULD run after resume but never does because app_var never becomes Resizing in the resume flow

- timestamp: 2026-02-28
  checked: Runner::run signatures and force_redraw parameter
  found: Runner::run("all") at section 5 timer tick has no_update=false, force_redraw=false (defaults). Without force_redraw, only diff_and_emit runs (line 800), not full_emit (line 797). Static elements not redrawn.
  implication: Even when Runner eventually collects new data, it won't redraw static UI chrome (box borders, labels, etc.)

- timestamp: 2026-02-28
  checked: What the old process_accumulated_state() did for resume
  found: The old enum-based system had process_accumulated_state() which would detect the Resume enum value and call _resume() directly, then transition to Running with a full screen rebuild. The new system relies on the event/state dispatch which has a gap.
  implication: The Phase 13 refactor lost the "resume triggers full redraw" behavior

## Resolution

root_cause: |
  The Ctrl+Z/fg resume flow has TWO interacting defects in the Phase 13 variant-based state machine:

  DEFECT 1: Missing on_event(Sleeping, Resume) handler.
  When the process resumes from SIGSTOP, app_var holds state::Sleeping{} (set during the Sleep transition).
  The Resume event is dispatched against Sleeping, but there is no on_event(Sleeping, Resume) overload.
  The catch-all at line 334 returns Sleeping{} (same state), so transition_to short-circuits with no actions.
  The comment on line 310 says "_resume() called in drain loop before dispatch" but the on_event
  it refers to is on_event(Running, Resume) which is NEVER reached because app_var is Sleeping, not Running.

  DEFECT 2: Section 2's Sleeping->Running transition overwrites the Resizing shadow flag.
  _resume() calls term_resize(true) which sets Global::app_state = Resizing. This COULD be caught
  by section 3's forced-resize check. But section 2 runs FIRST: it sees app_var is Sleeping and
  transitions Sleeping->Running. transition_to() at line 1010 sets Global::app_state = Running,
  erasing the Resizing flag. When section 3 checks Global::app_state == Resizing, it's false.

  DEFECT 3: on_enter(Running) is empty.
  Even though the Sleeping->Running transition fires correctly, on_enter(Running) at line 991-993
  does nothing. There is no Draw::calcSizes(), no Runner::run("all", true, true), no screen rebuild.

  NET RESULT: After resume, Terminal is re-initialized (_resume works), but the screen is never
  fully redrawn. Runner eventually restarts via the timer tick with Runner::run("all") but without
  force_redraw=true, only diff-based rendering occurs. Static elements (box borders, labels, etc.)
  that were wiped when the terminal was restored/re-initialized are never repainted.

  WHY RESIZE FIXES IT: A terminal resize triggers event::Resize{}, which transitions Running->Resizing.
  on_enter(Resizing) contains the full redraw logic (Draw::calcSizes, Runner::run("all", true, true)).

fix: [not applying -- diagnosis only]
verification: []
files_changed: []
