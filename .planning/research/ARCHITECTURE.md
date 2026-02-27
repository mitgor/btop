# Architecture Research

**Domain:** C++ terminal system monitor -- performance optimization of btop++
**Researched:** 2026-02-27
**Confidence:** HIGH (based on direct source code analysis of the repository)

## System Overview

```
+===================================================================+
|                        MAIN THREAD (btop.cpp)                      |
|  btop_main() -> init -> signal handlers -> MAIN LOOP              |
|                                                                    |
|  Main Loop:                                                        |
|    - Check thread exceptions, quit/sleep signals                   |
|    - Handle terminal resize (SIGWINCH)                             |
|    - Trigger Runner at update_ms intervals                         |
|    - Input::poll() with timeout until next update cycle            |
+===================================================================+
         |  thread_trigger()                  ^  Input::interrupt()
         v  (binary_semaphore)                |  (SIGUSR1)
+===================================================================+
|                    RUNNER THREAD (btop.cpp Runner::_runner)         |
|  THREAD LOOP:                                                      |
|    wait for trigger -> atomic_lock(active)                         |
|    For each box in ["cpu","mem","net","proc","gpu"]:               |
|      1. COLLECT: Xxx::collect(no_update) -> xxx_info&              |
|      2. DRAW:    Xxx::draw(info, force_redraw, data_same) -> str   |
|      3. APPEND:  output += draw_result                             |
|    cout << output << flush                                         |
+===================================================================+
         |                                    |
   +=====v=====+                        +=====v=====+
   |  COLLECT   |                        |   DRAW    |
   |  (platform)|                        | (btop_    |
   |            |                        |  draw.cpp)|
   | linux/     |                        |           |
   |  btop_     |   xxx_info structs     | Graph     |
   |  collect   | --------------------> | Meter     |
   |  .cpp      |   (cpu_info,          | createBox |
   |            |    mem_info,          | fmt::     |
   | osx/       |    net_info,          |  format   |
   |  btop_     |    proc_info[],       |           |
   |  collect   |    gpu_info[])        | Terminal  |
   |  .cpp      |                        | escape    |
   |            |                        | sequences |
   | freebsd/   |                        |           |
   |  btop_     |                        | String    |
   |  collect   |                        | concat    |
   |  .cpp      |                        | to output |
   +============+                        +===========+
         |                                    |
   +=====v=====+                        +=====v=====+
   | OS APIs    |                        | SUPPORT   |
   |            |                        |           |
   | /proc/stat |                        | Theme     |
   | /proc/pid  |                        | (colors,  |
   | sysctl     |                        |  grads)   |
   | IOKit      |                        |           |
   | getifaddrs |                        | Tools     |
   | /sys/class |                        | (string   |
   |            |                        |  utils)   |
   +============+                        +===========+
```

### Component Responsibilities

| Component | Responsibility | Key Files | Lines |
|-----------|----------------|-----------|-------|
| **Main/Init** | CLI parsing, config init, signal handling, main event loop, thread coordination | `btop.cpp` (1207L), `btop_cli.cpp` (272L), `main.cpp` (13L) | ~1,500 |
| **Runner** | Secondary thread that sequences collection and drawing; manages thread lifecycle | `btop.cpp` (Runner namespace, ~460L) | ~460 |
| **Config** | Configuration parsing, read/write, lock/unlock for thread safety, typed getters | `btop_config.cpp` (873L), `btop_config.hpp` (141L) | ~1,000 |
| **Data Collection** | Platform-specific system metric gathering (CPU, Mem, Net, Proc, GPU) | `linux/btop_collect.cpp` (3341L), `osx/btop_collect.cpp` (1981L), `freebsd/btop_collect.cpp`, etc. | ~3,300 per platform |
| **Drawing/Rendering** | Convert collected data into terminal escape-sequence strings; Graph/Meter classes; box layout | `btop_draw.cpp` (2517L), `btop_draw.hpp` (141L) | ~2,660 |
| **Menu** | Interactive menu overlay (settings, help, etc.) | `btop_menu.cpp` (1872L), `btop_menu.hpp` (99L) | ~1,970 |
| **Input** | Keyboard/mouse polling via pselect/ppoll, key decoding, action dispatch | `btop_input.cpp` (638L), `btop_input.hpp` (78L) | ~716 |
| **Theme** | Color/gradient generation, theme file loading, escape sequence caching | `btop_theme.cpp` (465L), `btop_theme.hpp` (70L) | ~535 |
| **Tools** | String manipulation (UTF-8 resize, justify, replace), terminal init, time functions, atomic helpers | `btop_tools.cpp` (669L), `btop_tools.hpp` (425L) | ~1,094 |
| **Shared** | Cross-platform data structures (cpu_info, mem_info, etc.), process sorting, tree generation | `btop_shared.cpp` (292L), `btop_shared.hpp` (470L) | ~762 |

## Recommended Project Structure

```
src/
+-- main.cpp                # Entry point (trivial)
+-- btop.cpp / btop.hpp     # btop_main(), Runner thread, signal handlers, main loop
+-- btop_cli.cpp/hpp        # CLI argument parsing
+-- btop_config.cpp/hpp     # Configuration management
+-- btop_draw.cpp/hpp       # Drawing pipeline: Graph, Meter, createBox, calcSizes, per-box draw()
+-- btop_input.cpp/hpp      # Input polling and action processing
+-- btop_menu.cpp/hpp       # Menu overlay rendering
+-- btop_shared.cpp/hpp     # Shared data structures, process sorting, tree generation
+-- btop_theme.cpp/hpp      # Theme/color management
+-- btop_tools.cpp/hpp      # Utility functions (string, time, terminal)
+-- btop_log.cpp/hpp        # Logging infrastructure
+-- config.h.in             # Build-time configuration template
+-- linux/
|   +-- btop_collect.cpp    # Linux data collection (3341L -- largest single file)
+-- osx/
|   +-- btop_collect.cpp    # macOS data collection (1981L)
|   +-- sensors.cpp/hpp     # macOS sensor reading
|   +-- smc.cpp/hpp         # macOS SMC interface
+-- freebsd/
|   +-- btop_collect.cpp    # FreeBSD data collection
+-- openbsd/
|   +-- btop_collect.cpp    # OpenBSD data collection
|   +-- sysctlbyname.cpp    # OpenBSD sysctl compatibility
+-- netbsd/
    +-- btop_collect.cpp    # NetBSD data collection
include/
+-- fmt/                    # Bundled fmt library (header-only via FMT_HEADER_ONLY)
+-- widechar_width.hpp      # Unicode character width tables
```

### Structure Rationale

- **Platform collectors isolated per directory:** Each OS has its own `btop_collect.cpp` implementing the same namespace interfaces (`Cpu::collect`, `Mem::collect`, etc.). This is the only platform-varying code; everything else is shared.
- **Flat core structure:** All core modules are siblings in `src/`. No abstraction layers, no interface classes, no virtual dispatch. Communication is via namespace-scoped global state and direct function calls.
- **Namespace-as-module pattern:** Each functional area is a C++ namespace (`Cpu`, `Mem`, `Net`, `Proc`, `Gpu`, `Draw`, `Config`, etc.). State is stored in namespace-scoped variables (effectively globals).

## Data Flow

### Per-Refresh-Cycle Data Flow (the Hot Path)

```
Timer expires (update_ms)
    |
    v
Main thread: Runner::run("all")
    |
    v  Config::unlock() -> Config::lock() -> copy config to runner_conf
    |  thread_trigger() (semaphore release)
    v
Runner thread wakes:
    |
    +---> Gpu::collect(no_update) -> reads GPU driver APIs -> vector<gpu_info>&
    |
    +---> Cpu::collect(no_update)
    |       +-- reads /proc/stat (Linux) or host_processor_info (macOS)
    |       +-- reads temperature sensors (/sys/class/hwmon or IOKit)
    |       +-- reads battery info (/sys/class/power_supply)
    |       +-- reads CPU frequency
    |       +-- returns cpu_info& (static, reused across cycles)
    |
    +---> Cpu::draw(cpu_info, gpus, force_redraw, data_same)
    |       +-- updates Graph objects (shifts data, generates braille/block chars)
    |       +-- generates colored escape sequences via Theme::g(), Theme::c()
    |       +-- builds string via fmt::format and string concatenation
    |       +-- appends to Runner::output
    |
    +---> Mem::collect(no_update)
    |       +-- reads /proc/meminfo (Linux) or host_statistics64 (macOS)
    |       +-- reads disk info (statvfs, /proc/diskstats)
    |       +-- returns mem_info& (static)
    |
    +---> Mem::draw(mem_info, ...) -> appends to Runner::output
    |
    +---> Net::collect(no_update)
    |       +-- reads /sys/class/net or getifaddrs()
    |       +-- returns net_info& (static)
    |
    +---> Net::draw(net_info, ...) -> appends to Runner::output
    |
    +---> Proc::collect(no_update)
    |       +-- iterates /proc/[pid]/ (Linux) or sysctl KERN_PROC (macOS)
    |       +-- for each process: reads /proc/[pid]/stat, /proc/[pid]/comm, /proc/[pid]/cmdline, /proc/[pid]/status
    |       +-- sorts processes (stable_sort)
    |       +-- optionally generates tree view (recursive tree_gen + tree_sort + prefix collection)
    |       +-- returns vector<proc_info>& (static, reused)
    |
    +---> Proc::draw(proc_info[], ...) -> appends to Runner::output
    |
    v
cout << output << flush  (single write to terminal)
    |
    v
Runner::active = false (notify main thread)
```

### Key Data Structures

| Structure | Defined In | Contents | Usage Pattern |
|-----------|-----------|----------|---------------|
| `Cpu::cpu_info` | `btop_shared.hpp` | `unordered_map<string, deque<long long>>` for percent fields, `vector<deque<long long>>` for cores/temps, load_avg | Static instance, reused each cycle. Deques grow to `width * 2` then pop_front. |
| `Mem::mem_info` | `btop_shared.hpp` | `unordered_map<string, uint64_t>` for stats, `unordered_map<string, deque<long long>>` for percent, `unordered_map<string, disk_info>` for disks | Static instance, reused each cycle. |
| `Net::net_info` | `btop_shared.hpp` | `unordered_map<string, deque<long long>>` bandwidth, `unordered_map<string, net_stat>` stat, ipv4/ipv6 strings | Static per-interface instances in `current_net` map. |
| `Proc::proc_info` | `btop_shared.hpp` | pid, name, cmd, short_cmd, threads, user, mem, cpu_p, cpu_c, state, ppid, tree fields | `vector<proc_info>` -- the full process list. Potentially thousands of entries. |
| `Draw::Graph` | `btop_draw.hpp` | width, height, `unordered_map<bool, vector<string>>` for double-buffered graph lines, color gradient | Created per graph instance. `operator()` called each cycle to append new data point. |
| `Draw::Meter` | `btop_draw.hpp` | width, `array<string, 101>` cache | 101-entry cache of pre-rendered meter strings. |

### State Management

```
Global namespace variables (btop.cpp)
    |
    +-- Global::quitting, Global::resized, Global::overlay, Global::clock (atomics + strings)
    |
Config namespace (btop_config.cpp)
    |
    +-- Three unordered_maps: strings, bools, ints
    +-- Lock/unlock mechanism: when locked, writes go to *Tmp maps; unlock merges
    +-- Thread safety via Config::lock()/unlock() around Runner cycles
    |
Per-box namespace state (btop_shared.hpp, btop_draw.cpp)
    |
    +-- Cpu::x, y, width, height, box (string), shown, redraw
    +-- Mem::x, y, width, height, box, shown, redraw
    +-- Net::x, y, width, height, box, shown, redraw
    +-- Proc::x, y, width, height, box, shown, redraw, p_graphs, p_counters
    |
Runner state
    |
    +-- Runner::active (atomic<bool>), Runner::stopping, Runner::output (string)
    +-- Runner::current_conf (copied from Config each cycle)
    +-- Binary semaphore for triggering work
```

## Hot Paths (Code That Runs Every Refresh Cycle)

### 1. Process Collection -- Proc::collect() [HIGHEST COST]

**Location:** `src/linux/btop_collect.cpp:2915-3324` (Linux), `src/osx/btop_collect.cpp:1663+` (macOS)

**What it does each cycle:**
- Iterates every `/proc/[pid]/` directory via `fs::directory_iterator`
- For each PID (potentially hundreds to thousands):
  - Opens and reads `/proc/[pid]/comm` (new processes only)
  - Opens and reads `/proc/[pid]/cmdline` (new processes only)
  - Opens and reads `/proc/[pid]/status` for UID (new processes only)
  - Opens and reads `/proc/[pid]/stat` **every cycle** -- parses specific fields with `ifstream` + `getline` + `stoull`
  - Optionally reads `/proc/[pid]/statm` if RSS looks wrong
- Uses `ifstream` for ALL file reads (heap allocation per open, buffered I/O overhead)
- Searches `current_procs` vector with `rng::find` (linear scan) per PID
- Calls `proc_sorter` which uses `rng::stable_sort`
- If tree mode: recursive `_tree_gen`, `tree_sort`, `_collect_prefixes`, then final sort

**Why expensive:**
- Hundreds/thousands of `ifstream` open/close per cycle (each involves heap allocation for internal buffer)
- `fs::directory_iterator` has overhead vs raw `opendir`/`readdir`
- Linear search in `current_procs` via `rng::find` for each PID
- String allocation for every PID path, field parsing
- `stoull`/`stoll` per field instead of direct numeric parsing
- Multiple sorts of the full process vector
- `std::regex` used in process filter matching (`matches_filter` calls `std::regex_search`)

### 2. Drawing Pipeline -- Xxx::draw() [HIGH COST]

**Location:** `src/btop_draw.cpp` -- functions at lines 540 (Cpu), 996 (Gpu), 1188 (Mem), 1454 (Net), 1569 (Proc)

**What it does each cycle:**
- Reads many config values via `Config::getB()`/`Config::getS()` (unordered_map lookups)
- Generates terminal output strings through heavy use of:
  - `fmt::format()` with named arguments (allocates intermediate strings)
  - String concatenation via `operator+=` (many small appends)
  - `Theme::c()` and `Theme::g()` lookups (unordered_map access for colors)
  - `Mv::to()`, `Mv::r()`, etc. (each creates a new string via `to_string`)
- `Graph::operator()` called for each graph:
  - Manipulates `vector<string>` graphs with `substr` and `erase` operations on each line
  - Builds output string with color gradient lookups per character
- For Proc::draw specifically:
  - Iterates visible processes, formats each row with `ljust`/`rjust` (allocates strings)
  - Creates per-process mini-graphs (`p_graphs` map)
  - Uses `floating_humanizer` for memory display (string operations + `fmt::format`)
  - Uses `uresize` for UTF-8 string truncation

**Why expensive:**
- Massive string allocation/concatenation -- output string can be tens of KB per frame
- `Mv::to(line, col)` creates a new string every call (escape sequence construction)
- `fmt::format` with named arguments is slower than positional
- `Theme::g(name).at(value)` -- double indirection through unordered_maps
- Per-process graph creation in Proc involves `Graph` constructor (initializes vectors, does computation)

### 3. CPU Collection -- Cpu::collect() [MODERATE COST]

**Location:** `src/linux/btop_collect.cpp:1050-1179` (Linux)

**What it does:**
- Opens `/proc/stat` with `ifstream`, parses CPU times per core
- Creates temporary `vector<long long>` for each core's times
- Multiple `unordered_map` lookups into `cpu_percent`, `cpu_old`
- Deque operations (push_back, pop_front) for time-series data

### 4. Memory/Disk Collection -- Mem::collect() [MODERATE COST]

**Location:** `src/linux/btop_collect.cpp:2042-2449` (Linux)

**What it does:**
- Reads `/proc/meminfo` via `ifstream`
- Iterates mounted filesystems via `fs::directory_iterator` on `/proc/diskstats`
- `statvfs` calls per disk
- Multiple `unordered_map` operations

### 5. String Utilities (called from everywhere) [MODERATE AGGREGATE COST]

**Location:** `src/btop_tools.cpp` and `src/btop_tools.hpp`

**Hot utility functions:**
- `ulen()` -- counts UTF-8 characters via `ranges::count_if` (called hundreds of times per frame)
- `uresize()` -- truncates UTF-8 strings (allocates new string each call, calls `shrink_to_fit`)
- `ljust()`/`rjust()`/`cjust()` -- pad strings (allocates per call)
- `s_replace()` -- creates new string, does in-place replacement loop
- `floating_humanizer()` -- converts bytes to human readable (multiple allocations)
- `trans()` -- replaces spaces with cursor-move escape codes
- `Fx::uncolor()` -- **uses std::regex_replace** (extremely expensive, called when menu overlay is active)

### 6. Output Flush [VARIABLE COST]

**Location:** `btop.cpp:728-731`

- Single `cout << output << flush` per cycle
- With `terminal_sync` enabled: wraps in `\e[?2026h` ... `\e[?2026l` (DEC private mode for synchronized output)
- Output string can be 10-100+ KB of escape sequences per frame

## Architectural Patterns

### Pattern 1: Collect-Then-Draw Sequential Pipeline

**What:** Each box is processed sequentially in the Runner thread: collect data, then draw to string. All boxes append to a single `Runner::output` string, which is flushed to the terminal at the end.

**Trade-offs:**
- Simple, no race conditions between boxes
- Serializes work that could be parallelized (CPU collection is independent of Mem collection)
- The single output string means one big terminal write (good for synchronized output)

### Pattern 2: Static-Return-Reference for Collection Data

**What:** All `collect()` functions return references to static local data structures (e.g., `static cpu_info current_cpu`). Data is mutated in place and the reference is passed to `draw()`.

**Trade-offs:**
- Zero copy overhead -- no allocation per cycle for the data structures themselves
- Data structures persist across cycles, which is intentional for time-series (deques maintain history)
- Not thread-safe -- relies on Runner being the only caller

### Pattern 3: Namespace-as-Module with Global State

**What:** Each functional area (Cpu, Mem, Net, Proc, etc.) is a C++ namespace with file-scoped and namespace-scoped globals. No classes, no virtual dispatch, no dependency injection.

**Trade-offs:**
- Simple, direct, zero overhead
- Makes testing hard (globals are implicit dependencies)
- Makes profiling easier (clear function boundaries)

### Pattern 4: Config Lock/Unlock for Thread Safety

**What:** `Config::lock()` redirects all `Config::set()` calls to temporary maps. `Config::unlock()` merges temporaries back. This happens around each Runner cycle.

**Trade-offs:**
- Avoids mutex on every config read (reads go direct to main maps)
- Two unordered_map copies per unlock is a small cost
- Potential for stale reads during collection/draw (acceptable -- values change rarely)

## Anti-Patterns (Optimization Targets)

### Anti-Pattern 1: ifstream for /proc File Reading

**What people do:** Use `std::ifstream` to read `/proc/[pid]/stat` and similar pseudo-files.
**Why it's wrong for performance:** `ifstream` constructor allocates a heap buffer (~4KB). With 500+ processes, that is 2000+ `ifstream` open/close cycles per refresh, each with heap allocation. `/proc` files are tiny (< 1KB) and can be read with a single `read()` syscall.
**Do this instead:** Use POSIX `open()`/`read()`/`close()` with a stack-allocated buffer. Or better, `openat()` with a pre-opened `/proc/[pid]` directory fd.

### Anti-Pattern 2: std::regex for Escape Code Stripping

**What people do:** `Fx::uncolor()` uses `std::regex_replace(s, color_regex, "")` to strip ANSI color codes.
**Why it's wrong:** `std::regex` is notoriously slow in libstdc++ and libc++. This processes the entire output string (potentially 50+ KB) character by character through a regex engine.
**Do this instead:** Hand-written state machine or simple loop that skips `\e[...m` sequences. (A commented-out version of this already exists in `btop_tools.cpp:189-210` but was disabled "due to issue when compiling with musl".)

### Anti-Pattern 3: Excessive Small String Allocations in Draw

**What people do:** Functions like `Mv::to(line, col)` create a new `std::string` each call via `Fx::e + to_string(line) + ';' + to_string(col) + 'f'`. This is called hundreds of times per frame.
**Why it's wrong:** Each call involves multiple heap allocations (SSO may help for small strings, but concatenation chains defeat it).
**Do this instead:** Write directly to the output buffer with `fmt::format_to(back_inserter)` or a pre-sized char buffer.

### Anti-Pattern 4: unordered_map for Fixed-Key Lookups

**What people do:** `cpu_info.cpu_percent` is `unordered_map<string, deque<long long>>` with fixed keys like "total", "user", "system". `Config` stores values in three `unordered_map`s.
**Why it's wrong:** Hash map lookups require hashing the key string and indirection through buckets. For a fixed, small set of keys, this is slower than an array or struct with named fields.
**Do this instead:** Use a struct with named fields, or an enum-indexed array. Eliminates hashing and pointer chasing.

### Anti-Pattern 5: Linear Search in Process List

**What people do:** For each PID during collection, `rng::find(current_procs, pid, &proc_info::pid)` does a linear scan of the entire process vector.
**Why it's wrong:** With N processes, this is O(N^2) total for the collection phase. On a system with 1000+ processes, this adds up.
**Do this instead:** Use a hash map (`unordered_map<size_t, size_t>` mapping PID to index) for O(1) lookup. Or keep the vector sorted by PID and use binary search.

### Anti-Pattern 6: fs::directory_iterator for /proc

**What people do:** `fs::directory_iterator(Shared::procPath)` to enumerate processes.
**Why it's wrong:** `fs::directory_iterator` constructs `fs::directory_entry` objects with `fs::path` (which heap-allocates). For `/proc` with 1000+ entries, this is significant overhead vs raw `opendir`/`readdir`.
**Do this instead:** Use POSIX `opendir`/`readdir` which returns `struct dirent` on the stack.

## Scaling Considerations (Optimization Impact by System Load)

| System Load | Primary Bottleneck | Optimization Priority |
|-------------|--------------------|-----------------------|
| Light (<100 processes) | Drawing pipeline (Graph, string building) | String allocation reduction, Graph optimization |
| Medium (100-500 processes) | Proc::collect (I/O) + Drawing | File I/O optimization, string pooling |
| Heavy (500-2000+ processes) | Proc::collect dominates (O(N^2) linear search, thousands of ifstream opens) | Replace ifstream with POSIX I/O, hash lookup for PIDs, reduce per-process allocations |
| All loads | Aggregate string allocation in draw + Mv::to() overhead | Buffer reuse, write-to-buffer pattern, eliminate intermediate strings |

### Scaling Priorities

1. **First bottleneck: Proc::collect file I/O** -- On systems with many processes, this is the single largest cost. Each process requires 1-4 file opens/reads per cycle, all through ifstream.
2. **Second bottleneck: Draw string allocation** -- The rendering pipeline generates enormous amounts of temporary strings. With 4-5 boxes, each with graphs, meters, and per-line formatting, the allocator is heavily exercised.
3. **Third bottleneck: Graph::_create / Graph::operator()** -- String manipulation inside graphs (substr, erase, color gradient lookup) has non-trivial cost.

## Suggested Optimization Order

Based on the architecture analysis, optimizations should be ordered by:
1. **Dependencies between components** -- changing data structures in btop_shared.hpp affects both collectors and drawing
2. **Risk level** -- file I/O changes are lower risk than data structure changes
3. **Impact breadth** -- string utility improvements affect all draw functions

### Recommended Phase Ordering

**Phase 1: Profiling Infrastructure**
- Add benchmarking harness that can measure collect vs draw time per box
- Establish baseline measurements before any changes
- No architectural changes -- just instrumentation
- *Dependency: None. Must come first.*

**Phase 2: String/Memory Allocation Reduction**
- Replace `Mv::to()` and similar with buffer-writing variants
- Introduce output buffer reuse pattern (pre-sized string, clear+reuse instead of allocate)
- Replace `Fx::uncolor()` regex with hand-written parser
- Replace `uresize()` shrink_to_fit pattern with in-place operations
- *Dependency: None. These are leaf-level utilities.*

**Phase 3: File I/O Optimization (Proc::collect)**
- Replace `ifstream` with POSIX `open`/`read`/`close` and stack buffers for `/proc` reading
- Replace `fs::directory_iterator` with `opendir`/`readdir`
- Replace linear PID lookup with hash-based lookup
- Batch `/proc/[pid]/stat` parsing with a single read + manual field extraction
- *Dependency: Phase 1 (profiling to measure improvement). Independent of Phase 2.*

**Phase 4: Data Structure Optimization**
- Replace `unordered_map<string, deque<long long>>` in cpu_info/mem_info with struct fields or enum-indexed arrays
- Consider replacing `deque<long long>` with ring buffers (fixed-size circular buffer)
- This touches `btop_shared.hpp` and ripples to ALL collectors and ALL draw functions
- *Dependency: Phase 1 (to justify). Phase 3 should come first because it's less risky.*

**Phase 5: Draw Pipeline Optimization**
- Reduce `fmt::format` overhead (switch to positional args or format_to)
- Reduce `Config::getB()`/`Config::getS()` lookups in draw functions (cache in local vars at start -- partly already done)
- Investigate Graph class double-buffering overhead
- Consider incremental/differential drawing (only redraw changed portions)
- *Dependency: Phase 4 (data structures must be stable). Phase 2 (string utilities must be optimized first).*

**Phase 6: Threading & Output**
- Consider parallelizing independent collections (Cpu, Mem, Net can run simultaneously)
- Investigate write batching / terminal output optimization
- Profile `cout << flush` vs `write()` for large output strings
- *Dependency: All prior phases. Highest risk, lowest certainty of gain.*

### Phase Dependency Graph

```
Phase 1 (Profiling) ----+---> Phase 2 (String/Alloc) --+---> Phase 5 (Draw Pipeline)
                        |                               |
                        +---> Phase 3 (File I/O) -------+---> Phase 6 (Threading)
                        |                               |
                        +---> Phase 4 (Data Structs) ---+
```

## Integration Points

### Internal Boundaries

| Boundary | Communication | Optimization Notes |
|----------|---------------|--------------------|
| Main thread <-> Runner thread | `binary_semaphore` + `atomic<bool>` active/stopping | Low overhead, well-designed |
| Config <-> All modules | `Config::getB/getI/getS` (unordered_map at()) | Called many times per cycle in draw; could cache locally |
| Collectors <-> Draw functions | Return static reference to data structs | Zero copy, good pattern. Data struct layout matters for cache. |
| Draw functions <-> Terminal | Single string concatenation, single `cout << flush` | Output string size is the main variable; terminal_sync wrapping helps |
| Theme <-> Draw functions | `Theme::c()` (string lookup), `Theme::g()` (gradient array lookup) | Called per-character in graphs, per-element in boxes. Hot path. |
| Tools (string utils) <-> All | `ulen()`, `uresize()`, `ljust()`, `rjust()`, `floating_humanizer()` | Called hundreds of times per frame. Allocation-heavy. |

### External Services (OS Interfaces)

| Service | Integration Pattern | Performance Notes |
|---------|---------------------|-------------------|
| `/proc` filesystem (Linux) | `ifstream` open/read/close per file | Main I/O bottleneck. Switch to POSIX read. |
| `sysctl` (macOS/BSD) | Direct syscall | Low overhead, fast |
| `IOKit` / `CoreFoundation` (macOS) | Framework calls for sensors, GPU | Moderate overhead, platform-specific |
| `getifaddrs()` (all platforms) | Single call returns linked list of all interfaces | Efficient, single syscall |
| GPU drivers (NVML, ROCm SMI) | Dynamic library loading, API calls | Moderate overhead, optional |
| Terminal I/O | `cout << flush` / `write(STDOUT_FILENO)` | Single large write per cycle is optimal pattern |

## Sources

- Direct source code analysis of btop++ repository at `/Users/mit/Documents/GitHub/btop/`
- `CMakeLists.txt` for build structure and platform conditionals
- `src/btop.cpp` for main loop, Runner thread, and signal handling (lines 462-735 for Runner, 844-1207 for main)
- `src/btop_draw.cpp` for rendering pipeline (2517 lines)
- `src/linux/btop_collect.cpp` for Linux data collection (3341 lines, largest file)
- `src/btop_tools.hpp/cpp` for string utility hot paths
- `src/btop_shared.hpp` for data structure definitions
- `src/btop_config.hpp` for configuration access patterns
- `src/btop_theme.hpp` for color/gradient lookup patterns

---
*Architecture research for: btop++ C++ terminal system monitor performance optimization*
*Researched: 2026-02-27*
