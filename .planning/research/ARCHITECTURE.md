# Architecture Research

**Domain:** C++ terminal system monitor — menu pushdown automaton + input FSM for btop++
**Researched:** 2026-03-02
**Confidence:** HIGH (direct source code analysis of the repository; existing FSM architecture verified)

---

## Context: What Already Exists (v1.2 Baseline)

v1.1/v1.2 shipped explicit FSMs for application lifecycle and runner thread. v1.3 extends the same
pattern into the menu system and input handling. Understanding the baseline is essential before
describing what changes.

### Existing FSM Components

**App FSM** (`btop_state.hpp`, `btop.cpp`)
- `AppStateVar` = `std::variant<state::Running, state::Resizing, state::Reloading, state::Sleeping, state::Quitting, state::Error>`
- `Global::app_state` atomic shadow tag for cross-thread reads
- `dispatch_event(AppStateVar, AppEvent) -> AppStateVar` + typed `on_event()` overloads
- `transition_to(current, next, ctx)` executes entry/exit actions, updates shadow

**Runner FSM** (`btop_state.hpp`, `btop_shared.hpp`)
- `Global::runner_state_tag` atomic shadow (`RunnerStateTag`: Idle/Collecting/Drawing/Stopping)
- Runner thread is main-thread-only for the variant; shadow used cross-thread

**Event Queue** (`btop_events.hpp`)
- Lock-free SPSC `EventQueue<AppEvent, 32>`
- `AppEvent` = `std::variant<TimerTick, Resize, Quit, Sleep, Resume, Reload, ThreadError, KeyInput>`
- Signal handlers push; main loop pops and dispatches

**Main loop input routing** (`btop.cpp:1572-1574`)
```cpp
if (Menu::active) Menu::process(key);
else              Input::process(key);
```
This `if/else` is the integration seam that the Input FSM replaces.

---

## Current Menu + Input State (To Be Replaced)

### Menu State Encoding (Implicit, Scattered)

| Variable | Location | Purpose | Problem |
|----------|----------|---------|---------|
| `atomic<bool> Menu::active` | `btop_menu.hpp` | Cross-thread "menu is open" flag | Implicit — reader must know what "active" implies |
| `bitset<8> menuMask` | `btop_menu.cpp` | Which menus are "live" | Bitset allows impossible combinations; ordering by iteration is fragile |
| `int currentMenu` | `btop_menu.cpp` | Which menu function gets input | Derived from `menuMask` scan; can desync from mask |
| Function-static locals | each menu function | Per-menu scroll/selection state | Lifetime tied to program; reset via sentinel (`bg.empty()`) |
| `menuReturnCodes` | `btop_menu.cpp` | NoChange/Changed/Closed/Switch | Already a PDA return protocol — just not typed |

**The 8 menus** (enum `Menu::Menus`): `SizeError`, `SignalChoose`, `SignalSend`, `SignalReturn`, `Options`, `Help`, `Renice`, `Main`

**Push path**: `Menu::show(menu, signal=-1)` → `menuMask.set(menu)` → `Menu::process()`
**Pop path**: `menuFunc[currentMenu](key)` returns `Closed` → `menuMask.reset(currentMenu)` → `process()` re-scans mask
**Switch path** (Main menu only): returns `Switch` → sets next `menuMask` bit and `currentMenu` directly

### Input State Encoding (Implicit, Flag-Driven)

| Mechanism | Location | Represents |
|-----------|----------|-----------|
| `Menu::active` check | `btop.cpp:1572` | MenuActive vs Normal/Filtering routing |
| `Config::proc_filtering` bool | `btop_input.cpp:221` | Filtering vs Normal input handling |
| `Input::process()` structure | `btop_input.cpp:218-643` | First handles `!filtering` globals, then per-box |

The implicit states are:
1. **Normal**: `!Menu::active && !proc_filtering` — global + per-box key handling
2. **Filtering**: `!Menu::active && proc_filtering` — text edit mode in proc box
3. **MenuActive**: `Menu::active` — all input forwarded to `Menu::process()`

---

## Target Architecture: v1.3

### System Overview

```
+=====================================================================+
|                    MAIN THREAD (btop.cpp)                           |
|                                                                     |
|  Signal EventQueue ──try_pop──► dispatch_event ──► transition_to   |
|                                 (App FSM, unchanged)                |
|                                                                     |
|  Input Poll ─────────────────────────────────────────────────────  |
|    Input::poll() → Input::get() → key string                        |
|         │                                                           |
|         ▼                                                           |
|  [NEW] InputFSM::process(key, input_var)                            |
|         │                                                           |
|    ┌────┴──────────────┐                                            |
|    │  InputStateVar    │ Normal / Filtering / MenuActive            |
|    └────┬──────────────┘                                            |
|         │ MenuActive branch                                         |
|         ▼                                                           |
|  [NEW] MenuPDA::process(key, pda_stack)                             |
|         │                                                           |
|    ┌────┴──────────────────────────────────────┐                    |
|    │  PDA stack: vector<MenuFrameVar>           │                    |
|    │                                            │                   |
|    │  [Main] [Options] [Help] [SignalChoose]... │                   |
|    │  top() = active frame; push/pop on events  │                   |
|    └───────────────────────────────────────────┘                    |
+=====================================================================+
         |  thread_trigger()                  ^  Input::interrupt()
         v  (binary_semaphore)                |  (SIGUSR1)
+=====================================================================+
|                    RUNNER THREAD                                     |
|  RunnerStateTag shadow (Idle/Collecting/Drawing/Stopping)            |
|  Unchanged from v1.2                                                 |
+=====================================================================+
```

### New Components

#### 1. Menu PDA (`btop_menu_pda.hpp`)

```
MenuFrameVar = std::variant<
    menu_frame::Main,
    menu_frame::Options,
    menu_frame::Help,
    menu_frame::SignalChoose,
    menu_frame::SignalSend,
    menu_frame::SignalReturn,
    menu_frame::SizeError,
    menu_frame::Renice
>
```

Each frame struct carries what was previously function-static locals:

```
menu_frame::Options {
    int y, x, height, page, pages;
    int selected, select_max, item_height, selected_cat, max_items, last_sel;
    bool editing;
    Draw::TextEdit editor;
    std::string warnings;
    std::bitset<8> selPred;   // internal to Options only, not global menuMask
}

menu_frame::Help {
    int y, x, height, page, pages;
}

menu_frame::SignalChoose {
    int x, y, selected_signal;
}

menu_frame::SignalSend {
    // stateless — s_pid read from Config each call
}

menu_frame::SignalReturn {
    // stateless — signalKillRet read from outer scope
}

menu_frame::SizeError {
    // stateless
}

menu_frame::Renice {
    int x, y, selected_nice;
    std::string nice_edit;
}

menu_frame::Main {
    int y, selected;
    std::vector<std::string> colors_selected, colors_normal;
}
```

The PDA itself:
```
struct MenuPDA {
    std::vector<MenuFrameVar> stack;   // top() = active menu

    bool empty() const;
    void push(MenuFrameVar frame);
    void pop();                        // pop active frame
    MenuFrameVar& top();
    const MenuFrameVar& top() const;

    // Process key against top frame; returns whether stack changed
    // Replaces menuFunc dispatch table
    PDAResult process(const std::string_view key);
};
```

`PDAResult` replaces `menuReturnCodes`:
```
enum class PDAResult { NoChange, Changed, Pop, Push };
```

#### 2. Input FSM (`btop_input_fsm.hpp`)

```
InputStateVar = std::variant<
    input_state::Normal,
    input_state::Filtering,
    input_state::MenuActive
>
```

State structs:
```
input_state::Normal   {}           // no per-state data needed
input_state::Filtering {
    std::string old_filter;         // was file-scoped 'old_filter' in btop_input.cpp
}
input_state::MenuActive {}          // no per-state data; menu state lives in MenuPDA
```

Typed dispatch replaces the `if/Menu::active` routing:
```cpp
InputStateVar on_input(const input_state::Normal&,    std::string_view key, MenuPDA& pda);
InputStateVar on_input(const input_state::Filtering&, std::string_view key, MenuPDA& pda);
InputStateVar on_input(const input_state::MenuActive&,std::string_view key, MenuPDA& pda);
```

Transitions:
- `Normal + "f"/"/"` → `Filtering{old_filter=current_filter}`
- `Normal + "escape"/"m"` → push `menu_frame::Main{}` → `MenuActive`
- `Normal + "f1"/"h"/"?"` → push `menu_frame::Help{}` → `MenuActive`
- `Normal + "f2"/"o"` → push `menu_frame::Options{}` → `MenuActive`
- `Filtering + "enter"/"escape"` → `Normal` (commit or discard filter)
- `MenuActive` + PDA becomes empty → `Normal`
- `MenuActive` + PDA push (Main→Options) → stay `MenuActive` (top changes)

### Modified Components

#### `btop_menu.hpp` / `btop_menu.cpp`

**Removed:**
- `atomic<bool> Menu::active` — replaced by `InputStateVar` holding `MenuActive`
- `bitset<8> menuMask` — replaced by PDA stack
- `int currentMenu` — replaced by `PDA::top()`
- Function-static locals in each menu function — moved into frame structs
- `menuFunc` dispatch vector — replaced by `std::visit` on `MenuFrameVar`
- `void Menu::show(int menu, int signal)` — replaced by `MenuPDA::push(frame)`

**Retained:**
- All rendering functions (the drawing logic stays the same, it just receives frame data instead of reading statics)
- `msgBox` class — unchanged
- `Menu::mouse_mappings` — unchanged, populated by active frame's render call
- `Menu::output`, `Menu::redraw`, `Menu::signalToSend`, `Menu::signalKillRet` — migrate as PDA fields or keep as file-scope

**Changed interface:**
```cpp
// OLD:
namespace Menu {
    extern atomic<bool> active;
    extern bitset<8> menuMask;
    void process(const std::string_view key = "");
    void show(int menu, int signal = -1);
}

// NEW:
namespace Menu {
    extern string output;   // retained
    extern bool redraw;     // retained
    // active, menuMask, currentMenu removed
}
```

#### `btop_input.hpp` / `btop_input.cpp`

**Removed:**
- File-scoped `string old_filter` — moved into `input_state::Filtering`
- Implicit `if (filtering)` branch at top of `Input::process()` — replaced by FSM dispatch

**Retained:**
- `Input::poll()`, `Input::get()`, `Input::wait()`, `Input::interrupt()`, `Input::clear()`
- `Input::mouse_mappings`, `Input::mouse_pos`, `Input::polling`, `Input::history`
- All per-box key handling logic within `Normal` state dispatch

**Changed:**
- `Input::process(key)` becomes `Input::FSM::process(key, input_var, pda)` or equivalent
- The function signature may stay the same externally with state passed as references

#### `btop.cpp` Main Loop

**Line 1572-1574 changes from:**
```cpp
if (Menu::active) Menu::process(key);
else              Input::process(key);
```

**To:**
```cpp
Input::FSM::process(key, input_var, menu_pda);
// InputFSM internally routes to MenuPDA when in MenuActive state
```

`input_var` (of type `InputStateVar`) replaces `Menu::active` and `proc_filtering` for routing decisions. These two pieces of state are now the Input FSM state, not separate booleans.

#### `btop_events.hpp`

No changes expected. The event queue and `AppEvent` types serve App FSM signals (SIGWINCH, SIGTERM, etc.). Input events (keystrokes) are handled synchronously in the main loop's input poll block, not via the async event queue — this separation is correct and should be preserved.

---

## Data Flow: Menu Push

```
User presses "m" or "escape" (not in menu):

1. Input::poll() returns true
2. Input::get() → key = "m"
3. InputFSM::process("m", input_var, pda):
   - current input_var = Normal{}
   - on_input(Normal{}, "m", pda):
       pda.push(menu_frame::Main{y=0, selected=0, ...})
       Menu::redraw = true
       Runner::pause_output = true
       return input_state::MenuActive{}
   - input_var = MenuActive{}

4. Next frame: input_var = MenuActive{}, so on_input(MenuActive{}, key, pda):
   - pda.top() = menu_frame::Main{...}
   - std::visit renders and processes key against Main frame
   - If user presses "Options": pda.push(menu_frame::Options{...}); return NoChange
   - If user presses "escape": pda.pop(); if pda.empty() → return input_state::Normal{}
```

## Data Flow: Menu Pop

```
User presses "escape" while in Options (inside Main):

1. InputFSM::process("escape", input_var, pda):
   - input_var = MenuActive{}
   - pda.top() = menu_frame::Options{...}
   - on_input(MenuActive{}, "escape", pda):
       auto result = pda.top().handle("escape")
       result == PDAResult::Pop → pda.pop()
       pda.top() = menu_frame::Main{...}  // back to Main
       pda is not empty → stay in MenuActive
       return input_state::MenuActive{}

2. Next keypress with Main on top:
   - User presses "escape" again → pda.pop()
   - pda.empty() → return input_state::Normal{}
   - Runner::pause_output = false
   - Runner::run("all", true, true)
```

## Data Flow: Filtering Entry/Exit

```
User presses "f" (Normal state, proc box shown):

1. on_input(Normal{}, "f", pda):
   - Config::flip(BoolKey::proc_filtering)  // retained for Config consumers
   - Proc::filter = Draw::TextEdit{Config::getS(StringKey::proc_filter)}
   - old_filter = Proc::filter.text
   - return input_state::Filtering{old_filter}

2. User presses "escape" while Filtering:
   on_input(Filtering{old_filter}, "escape", pda):
   - Config::set(StringKey::proc_filter, old_filter)
   - Config::set(BoolKey::proc_filtering, false)
   - return input_state::Normal{}
```

Note: `Config::proc_filtering` bool is retained as a Config value because the drawing pipeline and
`Input::get()` mouse routing both read it. The Input FSM adds a typed wrapper; it does not eliminate
the config value.

---

## Component Boundaries

| Component | Owns | Communicates With | Notes |
|-----------|------|-------------------|-------|
| `MenuPDA` | stack of `MenuFrameVar`, per-frame state | `Menu::` rendering functions, `Input::mouse_mappings` | Main-thread only |
| `InputStateVar` | current input mode (Normal/Filtering/MenuActive) | `MenuPDA` (when MenuActive) | Main-thread only |
| `AppStateVar` | app lifecycle state (Running/Resizing/...) | Event queue, `transition_to()` | Independent of menu/input FSMs |
| `RunnerStateTag` | runner thread phase | Shadow atomic for main-thread reads | Unchanged from v1.2 |
| `Menu::output` | rendered menu overlay string | Runner writes to terminal overlay | Retained, written by frame render |
| `Config::proc_filtering` | text filter mode flag | Drawing pipeline, `Input::get()` mouse logic | Retained as Config bool; set by Input FSM |

---

## Build Order (Phase Dependencies)

The dependency graph follows the same "outside-in" principle as v1.1:

```
Phase A: MenuFrameVar types + PDA stack
   (no UI changes, just data structures)
         |
         v
Phase B: Migrate function-statics into frame structs
   (behavior-preserving, renders using frame data)
         |
         v
Phase C: Replace menuMask/currentMenu/menuFunc with PDA dispatch
   (replaces bitset dispatch; menuMask removed)
         |
         v
Phase D: Input FSM (InputStateVar)
   (replaces if/Menu::active routing; depends on C because
    MenuActive must be stable before InputFSM can delegate to PDA)
         |
         v
Phase E: Tests (PDA transitions + Input FSM transitions)
   (depends on D for testable interfaces)
         |
         v
Phase F: Cleanup (remove Menu::active, old_filter, menuMask declarations)
   (depends on E confirming correctness)
```

**Why this order:**
1. **A before B**: Can't populate frame structs before they exist
2. **B before C**: The rendering code must consume frame data before dispatch is switched
3. **C before D**: InputFSM's MenuActive branch delegates to PDA; PDA must be the dispatch authority first
4. **D before E**: Tests exercise the full InputFSM + PDA integration
5. **E before F**: Cleanup only after tests confirm the old state variables are unused

---

## Architectural Patterns

### Pattern 1: PDA via `std::variant` Stack

**What:** `std::vector<MenuFrameVar>` with push/pop. `std::visit` dispatches input to active frame.
**When to use:** When the system must return to prior context (menu → submenu → menu).
**Trade-offs:**
- `std::visit` on variant is a switch/jump table — equivalent to `menuFunc.at(currentMenu)(key)`
- Frame structs carry per-frame state; no function-static lifetime issues
- Stack is heap-allocated (vector), but only modified on menu open/close (not hot path)

```cpp
// Dispatch input to active frame
PDAResult MenuPDA::process(std::string_view key) {
    if (stack.empty()) return PDAResult::Pop;
    return std::visit(
        [&](auto& frame) { return frame.handle(key); },
        stack.back()
    );
}
```

### Pattern 2: Input FSM Owns Mode, PDA Owns Stack

**What:** `InputStateVar` holds the current input mode. When `MenuActive`, it delegates to `MenuPDA` but does not hold the PDA itself.
**Why:** Separates "what kind of input are we doing" from "which menu is active". The Input FSM transition back to `Normal` happens when the PDA stack empties — this is the only coupling point.

```cpp
InputStateVar on_input(const input_state::MenuActive& s,
                       std::string_view key, MenuPDA& pda) {
    auto result = pda.process(key);
    if (pda.empty()) {
        // Exit menu mode: restore runner output, trigger redraw
        Runner::pause_output.store(false);
        Runner::run("all", true, true);
        Menu::output.clear();
        return input_state::Normal{};
    }
    return s;  // stay in MenuActive
}
```

### Pattern 3: Entry Actions on PDA Push

**What:** Each `MenuFrameVar::push()` performs the frame's entry action inline (set `Menu::redraw = true`, clear `bg`, configure `Runner::pause_output`).
**Why:** Mirrors the existing `Menu::show()` + `Menu::process()` flow where the first render cycle detects a new menu from `menuMask`. Makes state transitions explicit.

### Pattern 4: Shadow Flag Retention

**What:** `Menu::active` (atomic bool) is retained as a derived value updated whenever `input_var` changes state, OR it is removed and all cross-thread readers are identified and updated.
**Trade-off:** Removing `Menu::active` is cleaner but requires auditing all readers. The main known cross-thread reader is none — `Menu::active` is only read on the main thread in `btop.cpp:1544` (`if (not Menu::active)` for clock update). This makes removal safe.
**Recommendation:** Remove `Menu::active`; replace the `btop.cpp:1544` check with `!std::holds_alternative<input_state::MenuActive>(input_var)`.

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Keeping `menuMask` as Internal PDA State

**What people do:** Wrap `bitset<8>` inside the PDA class, keeping bitset semantics.
**Why it's wrong:** Defeats the purpose. The bitset allows multiple bits set simultaneously — the invalid-state problem being solved. A stack enforces exactly one active menu.
**Do this instead:** `std::vector<MenuFrameVar>` stack. Only `top()` is active. Multiple menus are represented by depth, not simultaneous bits.

### Anti-Pattern 2: Putting `old_filter` in Global Scope

**What people do:** Leave `old_filter` as a file-scoped variable in `btop_input.cpp`.
**Why it's wrong:** `old_filter` is per-filtering-session data — it belongs with the Filtering state.
**Do this instead:** `input_state::Filtering{ std::string old_filter; }` carries it with the state.

### Anti-Pattern 3: Bypassing the Input FSM for Menu Access

**What people do:** Call `Menu::show(Menu::Help)` directly from places other than the Input FSM.
**Why it's wrong:** Direct calls bypass the `InputStateVar` transition — `input_var` stays `Normal` while PDA has frames, corrupting the invariant that `MenuActive` iff `!pda.empty()`.
**Do this instead:** All menu activations go through `InputFSM::push_menu(frame)` which atomically pushes and transitions `input_var` to `MenuActive`.

### Anti-Pattern 4: Frame Render Functions Taking Raw `currentMenu` Int

**What people do:** Pass `currentMenu` or enum int to render functions instead of frame data.
**Why it's wrong:** Reverts to the implicit coupling the PDA eliminates.
**Do this instead:** Each frame struct's `render(std::string& overlay)` method is called by the PDA; the frame knows its own data.

### Anti-Pattern 5: Routing Menu Entry Through the Event Queue

**What people do:** Push a `MenuOpen` event into `Global::event_queue` to trigger menu display.
**Why it's wrong:** The event queue is for signal-handler-to-main-loop communication (async, signal-safe). Menu activation is synchronous main-thread work triggered by keystrokes; it does not cross thread or signal boundaries.
**Do this instead:** Menu activation is a direct synchronous transition inside `InputFSM::process()`.

---

## Integration Points

### Modified Internal Boundaries

| Boundary | Before v1.3 | After v1.3 |
|----------|-------------|------------|
| Main loop → Input routing | `if (Menu::active) Menu::process(key); else Input::process(key)` | `InputFSM::process(key, input_var, pda)` |
| Menu → Input | `Menu::active` atomic bool checked in `Input::get()` for mouse routing | `std::holds_alternative<input_state::MenuActive>(input_var)` |
| Input → Menu activation | `Menu::show(int menu, int signal)` called from `Input::process()` | `pda.push(make_frame(menu, signal))` called from `on_input(Normal, key, pda)` |
| Menu internal dispatch | `menuFunc.at(currentMenu)(key)` via function pointer vector | `std::visit([&](auto& f){ f.handle(key); }, pda.top())` |
| Per-frame state lifetime | Function-static locals (program lifetime) | Frame struct fields (stack lifetime) |

### Unchanged Internal Boundaries

| Boundary | Notes |
|----------|-------|
| App FSM event queue | Signals → queue → `dispatch_event` → `transition_to`. No changes. |
| Runner FSM | `RunnerStateTag` shadow, `pause_output`, semaphore. No changes. |
| Config lock/unlock | Per-cycle around Runner. No changes. |
| Drawing pipeline | `Xxx::draw()`, `Graph`, `Meter`. No changes. |
| `Input::poll()` / `Input::get()` | Low-level I/O unchanged. `get()` checks `Config::proc_filtering` for mouse routing — this Config bool is retained. |

---

## Test Strategy

Following the v1.1 pattern: tests exercise state machines in isolation without starting the terminal or runner thread.

**PDA tests** (`tests/test_menu_pda.cpp`):
- Push/pop invariants (push → `!empty()`, pop until empty → `empty()`)
- Frame isolation (Options frame data does not bleed to Help frame)
- SizeError detection and push on resize
- SignalSend → SignalReturn push on kill failure
- Main → Options → pop → Main frame data preserved

**Input FSM tests** (`tests/test_input_fsm.cpp`):
- `Normal + "f" → Filtering` with `old_filter` captured
- `Filtering + "escape" → Normal` with filter restored
- `Normal + "m" → MenuActive` (via PDA push)
- `MenuActive + pda empties → Normal` (exit cleanup)
- Mouse routing: `MenuActive` checks `pda.top()` mouse_mappings, not `Menu::active`

**Both test suites** must run under ASan+UBSan and TSan (same CMake targets as v1.1).

---

## Sources

- Direct source code analysis: `src/btop_menu.hpp`, `src/btop_menu.cpp`, `src/btop_input.hpp`, `src/btop_input.cpp`
- Existing FSM infrastructure: `src/btop_state.hpp`, `src/btop_events.hpp`, `src/btop.cpp`
- Existing test patterns: `tests/test_app_state.cpp`, `tests/test_transitions.cpp`, `tests/test_events.cpp`
- v1.2 PROJECT.md context: `.planning/PROJECT.md`
- Prior research: `.planning/research/AUTOMATA-ARCHITECTURE.md`

---
*Architecture research for: btop++ v1.3 menu PDA + input FSM*
*Researched: 2026-03-02*
