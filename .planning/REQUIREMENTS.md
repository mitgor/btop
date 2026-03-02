# Requirements: btop++ v1.3

**Defined:** 2026-03-02
**Core Value:** Achieve measurable, significant reductions in btop's own resource consumption while evolving the architecture toward explicit, testable state machines that eliminate invalid state combinations.

## v1.3 Requirements

Requirements for the Menu PDA + Input FSM milestone. Each maps to roadmap phases.

### Menu PDA Infrastructure

- [x] **PDA-01**: MenuFrameVar std::variant defined with alternatives for all 8 menus (Main, Options, Help, SizeError, SignalChoose, SignalSend, SignalReturn, Renice)
- [x] **PDA-02**: MenuPDA class provides push(), pop(), replace(), top(), empty() operations on std::stack<MenuFrameVar, std::vector<MenuFrameVar>>
- [x] **PDA-03**: Frame handlers return PDAAction enum (Push/Pop/Replace/NoChange) instead of modifying stack during std::visit
- [x] **PDA-04**: PDA dispatch replaces menuMask bitset + currentMenu int + menuFunc dispatch vector
- [x] **PDA-05**: bg string lifecycle tied to stack depth (captured on first push, cleared on final pop)
- [x] **PDA-06**: replace() operation handles Main→Options and Main→Help transitions (ESC returns to Normal, not Main)

### Frame Data

- [x] **FRAME-01**: Each frame struct carries per-frame state as members (replacing function-static locals)
- [x] **FRAME-02**: Frame structs split into layout fields (x, y, width, height) and interaction fields (selected, page, entered text)
- [x] **FRAME-03**: invalidate_layout() zeros layout fields while preserving interaction fields
- [x] **FRAME-04**: Frame constructors unconditionally initialize all fields (eliminating bg.empty() reset sentinel)
- [x] **FRAME-05**: Per-frame mouse_mappings owned by frame struct (only top frame's mappings active)

### Input FSM

- [ ] **INPUT-01**: InputStateVar std::variant defined with Normal, Filtering, and MenuActive states
- [ ] **INPUT-02**: Filtering state carries old_filter as struct member (replacing file-scoped global)
- [ ] **INPUT-03**: All input transitions typed: Normal↔Filtering, Normal↔MenuActive
- [ ] **INPUT-04**: Input::process() dispatches via InputStateVar instead of if(filtering)/if(Menu::active) branches
- [ ] **INPUT-05**: Menu::active atomic bool removed — InputStateVar is sole authority for input routing
- [ ] **INPUT-06**: Mouse routing in Input::get() reads InputStateVar instead of Menu::active bool

### Integration

- [ ] **INTEG-01**: App FSM on_enter(Resizing) calls menu_pda.invalidate_layout() to prevent stale coordinates
- [ ] **INTEG-02**: Runner::pause_output cleared on all paths that empty the PDA stack
- [ ] **INTEG-03**: Config::proc_filtering retained for display/persistence but FSM is routing authority
- [ ] **INTEG-04**: Main loop btop.cpp:1572-1574 replaced with InputFSM-based dispatch

### Testing

- [ ] **TEST-01**: PDA transition tests: push/pop/replace invariants, frame isolation, reopen-fresh-state
- [ ] **TEST-02**: Input FSM transition tests: all state transitions, key routing per state, mouse routing
- [ ] **TEST-03**: SizeError override test (push over existing menu)
- [ ] **TEST-04**: SignalSend→SignalReturn sequence test (post-pop push pattern)
- [ ] **TEST-05**: Resize-with-menu-open preserves interaction, invalidates layout
- [ ] **TEST-06**: All Filtering exit paths tested (keyboard ESC, mouse click, enter)
- [ ] **TEST-07**: ASan + UBSan + TSan clean builds; all existing tests passing

### Cleanup

- [ ] **CLEAN-01**: menuMask bitset, currentMenu int, menuFunc vector removed
- [ ] **CLEAN-02**: Menu::active atomic bool declaration removed from btop_menu.hpp
- [ ] **CLEAN-03**: old_filter file-scope variable removed from btop_input.cpp
- [ ] **CLEAN-04**: All function-static locals in menu functions removed

## v1.4 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Advanced State Machine Features

- **ADV-01**: Transition logging infrastructure for menu PDA and input FSM
- **ADV-02**: Stack depth defensive guard (assert stack.size() <= 4)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Deep push of Main under Options/Help | ESC from Options must return to Normal, not Main — this would be a regression |
| Async/coroutine menu state machines | Would introduce races with Runner's pause_output and bg string |
| External FSM libraries (Boost.SML, TinyFSM) | Violates no-new-dependencies constraint |
| UI/UX changes | Architecture-only refactor, zero user-visible changes |
| Input state machine beyond Normal/Filtering/MenuActive | Three states cover all existing input modes |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| PDA-01 | Phase 20 | Complete |
| PDA-02 | Phase 20 | Complete |
| PDA-03 | Phase 20 | Complete |
| PDA-04 | Phase 22 | Complete |
| PDA-05 | Phase 20 | Complete |
| PDA-06 | Phase 22 | Complete |
| FRAME-01 | Phase 21 | Complete |
| FRAME-02 | Phase 20 | Complete |
| FRAME-03 | Phase 21 | Complete |
| FRAME-04 | Phase 20 | Complete |
| FRAME-05 | Phase 20 | Complete |
| INPUT-01 | Phase 23 | Pending |
| INPUT-02 | Phase 23 | Pending |
| INPUT-03 | Phase 23 | Pending |
| INPUT-04 | Phase 23 | Pending |
| INPUT-05 | Phase 23 | Pending |
| INPUT-06 | Phase 23 | Pending |
| INTEG-01 | Phase 23 | Pending |
| INTEG-02 | Phase 23 | Pending |
| INTEG-03 | Phase 23 | Pending |
| INTEG-04 | Phase 23 | Pending |
| TEST-01 | Phase 24 | Pending |
| TEST-02 | Phase 24 | Pending |
| TEST-03 | Phase 24 | Pending |
| TEST-04 | Phase 24 | Pending |
| TEST-05 | Phase 24 | Pending |
| TEST-06 | Phase 24 | Pending |
| TEST-07 | Phase 24 | Pending |
| CLEAN-01 | Phase 25 | Pending |
| CLEAN-02 | Phase 25 | Pending |
| CLEAN-03 | Phase 25 | Pending |
| CLEAN-04 | Phase 25 | Pending |

**Coverage:**
- v1.3 requirements: 32 total
- Mapped to phases: 32
- Unmapped: 0

---
*Requirements defined: 2026-03-02*
*Last updated: 2026-03-02 after roadmap creation*
