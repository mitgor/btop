# Feature Research: Performance Optimization Categories

**Domain:** C++ terminal system monitor (btop++) performance optimization
**Researched:** 2026-02-27
**Confidence:** HIGH (based on codebase analysis and established C++ optimization principles)

## Feature Landscape

### Table Stakes (Any Competent Optimizer Would Do These)

Foundational optimizations that eliminate waste. These are low-risk, high-confidence wins that any profiling session would reveal. Missing these would indicate the optimization effort is not serious.

| Feature | Why Expected | Complexity | Expected Impact | Notes |
|---------|--------------|------------|-----------------|-------|
| Replace `std::regex` with manual parsing or CTRE | `std::regex` is 10-100x slower than alternatives; used in hot paths (`Fx::uncolor`, `Fx::escape_regex`, proc filter matching in `btop_shared.cpp`) | LOW | 5-15% CPU reduction in draw path; process filter matching becomes near-instant | `Fx::uncolor()` is called per-frame via `Fx::uncolor(output)` in the runner thread output path (line 730 of `btop.cpp`). The disabled manual parser in `btop_tools.cpp:187-210` was the right idea -- just needs musl compatibility fix |
| Eliminate unnecessary string copies | Functions like `uresize(string str, ...)` and `luresize(string str, ...)` take strings by value instead of `const string&` or `string_view`; `s_replace` copies input; `ljust/rjust/cjust` take by value | LOW | 3-8% CPU reduction overall; reduced heap allocations | `btop_tools.hpp:179-182` shows `uresize` and `luresize` taking `const string str` by value -- these are called thousands of times per frame in draw code |
| Use `string::reserve()` in draw functions | Draw functions build output strings character-by-character with `+=`. `Proc::draw` does `out.reserve(width * height)` but most others do not | LOW | 2-5% reduction in draw time; fewer heap reallocations | `Cpu::draw`, `Mem::draw`, `Net::draw`, `Gpu::draw` all build large output strings without reserving. The output string in the runner thread (`output.clear()`) also never reserves |
| Replace `std::to_string()` in hot loops with `std::to_chars()` | `to_string()` uses locale and allocates; `to_chars()` writes to a stack buffer with zero allocation | LOW | 1-3% CPU; measurable in per-process draw loops | Used extensively in `Mv::to()`, `Mv::r()`, `Mv::l()` etc. in `btop_tools.hpp:104-116` -- these are called thousands of times per frame |
| Use `std::string_view` for config lookups | `Config::getS()` returns `const string&` which is fine, but many callers copy the result into `string` variables | LOW | 1-2% CPU; reduced temporary allocations | Throughout draw functions: `auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_proc"))` is good; but many other call sites copy |
| Switch `/proc` file reads from `ifstream` to raw `open()/read()` | Process collection opens 3-5 files per PID using `ifstream`, which has constructor/destructor overhead and buffering overhead for tiny files (< 4KB) | MEDIUM | 10-25% reduction in Proc::collect time | `linux/btop_collect.cpp` lines 2935-3184: opens `comm`, `cmdline`, `status`, `stat`, and sometimes `statm` per process. Raw `read()` into a stack-allocated buffer avoids heap allocation entirely |
| Replace `readfile()` with direct `read()` for small sysfs files | `readfile()` in `btop_tools.cpp:561-573` checks `fs::exists()` then opens `ifstream` and reads line-by-line -- 3 syscalls minimum for a file that's typically < 100 bytes | LOW | 5-10% reduction in sensor/battery/network collection time | Called dozens of times per update cycle for sensors, battery, network stats. A single `open()+read()+close()` with stack buffer would suffice |
| Avoid `fs::exists()` before `open()` | `readfile()` calls `fs::exists()` which is an extra `stat()` syscall before the `open()` -- just try to open and handle failure | LOW | Minor syscall reduction (dozens per cycle) | TOCTOU issue too: file could disappear between exists() and open() |
| Pre-compute static escape sequences | `Mv::to()`, `Mv::r()`, `Mv::l()` etc. dynamically build escape strings every call. For fixed positions (box corners, headers), these could be computed once | LOW | 1-3% draw time reduction | Box layouts change only on resize. Cache escape sequences in `calcSizes()` and reuse |
| Use `fmt::format_to` with `std::back_inserter` instead of `fmt::format` | `fmt::format` returns a new string; `fmt::format_to` appends directly to an existing buffer, avoiding allocation | LOW | 2-4% in draw functions | Already used in a few places in `Proc::draw` (lines 1830, 1887) but not consistently throughout |

### Differentiators (Advanced Techniques -- Competitive Advantage)

These go beyond obvious fixes. They require deeper architectural understanding and yield compounding benefits. Implementing these would make btop++ demonstrably best-in-class in efficiency.

| Feature | Value Proposition | Complexity | Expected Impact | Notes |
|---------|-------------------|------------|-----------------|-------|
| Direct `/proc` parsing with `open()+read()` into fixed stack buffers | Eliminate all heap allocation in the process collection hot loop. Use `char buf[4096]` on the stack (page-aligned for /proc), parse in-place with `from_chars()` | MEDIUM | 20-40% reduction in Proc::collect CPU time | This is the single highest-impact optimization. The current code does ~5 `ifstream` open/close cycles per process, each involving heap allocation. On a system with 500 processes, that's 2500+ heap allocations per update. A fixed stack buffer with `read()` reduces this to zero heap allocations |
| Differential/incremental terminal output | Instead of rebuilding the entire frame string each cycle, track what changed and emit only the delta escape sequences | HIGH | 30-50% reduction in draw time and terminal I/O | Currently the runner thread rebuilds the entire output string (~10-50KB) every cycle. Terminal emulators already handle cursor-addressed updates efficiently. Only redraw cells that actually changed (new value != old value) |
| Process data structure optimization: `vector` with index lookup instead of `unordered_map` | Replace `unordered_map<string, deque<long long>>` in `cpu_info`, `mem_info`, `net_info`, `gpu_info` with flat `array` or `vector` indexed by enum | MEDIUM | 10-20% reduction in collection + draw time | `cpu_percent`, `mem.stats`, `mem.percent`, `net.bandwidth` all use `unordered_map<string, ...>` with string keys like `"total"`, `"user"`, `"idle"`. Replace with `enum class CpuField { Total, User, Idle, ... }` and `array<deque<long long>, N>`. Eliminates hashing + string comparison on every access |
| Ring buffer instead of `deque<long long>` for time-series data | `deque` allocates in chunks and has poor cache locality. A fixed-size ring buffer (`array<long long, N>`) is contiguous and avoids all heap allocation | MEDIUM | 5-15% reduction in graph data management | Used everywhere: `cpu_percent`, `core_percent`, `temp`, `gpu_percent`, `bandwidth`, `io_read`, etc. Graph width determines max entries needed -- typically 200-300 values max. A `std::array<long long, 512>` ring buffer with head/tail indices would be zero-allocation and cache-friendly |
| Batch terminal writes with `writev()` | Instead of building one giant string and calling `cout << output << flush`, use `writev()` with scatter-gather I/O to write multiple buffers in a single syscall | MEDIUM | 5-10% reduction in I/O overhead; avoids building the concatenated output string | The runner thread concatenates cpu+gpu+mem+net+proc output strings into one big `output` string, then writes it all at once. With `writev()`, each box's output could remain separate -- no final concatenation needed |
| Pool-allocate `proc_info` objects | `proc_info` contains 7 `string` members (name, cmd, short_cmd, user, prefix, plus state info). New processes trigger heap allocations for each string. Use a memory pool or arena allocator | HIGH | 5-15% reduction in Proc::collect allocation overhead | `current_procs` is a `vector<proc_info>` that grows/shrinks as processes appear/disappear. String members cause heap fragmentation. An arena that recycles `proc_info` slots would eliminate per-process allocation churn |
| Move graph symbol lookup from `unordered_map` to flat array | `Symbols::graph_symbols` is `unordered_map<string, vector<string>>` with 6 entries, looked up by string key on every graph update | LOW | 1-3% draw time reduction | Replace with `enum class GraphSymbol { BrailleUp, BrailleDown, ... }` and `array<array<string, 25>, 6>`. Eliminates string hashing in hot graph rendering loop |
| Compile-time escape sequence generation with `constexpr` | Many escape sequences in `Fx`, `Term`, `Mv` namespaces are runtime-constructed `string` objects. Convert to `constexpr string_view` or `constexpr char[]` | LOW | 1-2% startup time + minor runtime improvement | `Fx::e`, `Fx::b`, `Fx::ub`, `Term::hide_cursor`, etc. are `const string` (heap-allocated at startup). Making them `constexpr const char[]` or `string_view` eliminates ~30 startup heap allocations |
| Parallel data collection | Collect CPU, memory, network, disk, and process data concurrently instead of sequentially in the runner thread | HIGH | 20-40% wall-clock reduction per update cycle (not CPU reduction -- spreads work across cores) | Currently the runner loop in `btop.cpp:514-651` collects CPU, then GPU, then MEM, then NET, then PROC sequentially. CPU and MEM/NET reads are independent and could run in parallel. Complexity: thread safety of shared state, increased total CPU usage |
| `charconv`-based number parsing for `/proc` files | Replace `stoull()`, `stoll()`, `stod()`, `stoi()` with `std::from_chars()` throughout proc collection | LOW | 3-8% reduction in Proc::collect time | `from_chars()` is locale-independent, allocates nothing, and is typically 2-5x faster than `sto*()` functions. Used ~10 times per process in the stat parsing loop |
| Lazy/on-demand proc detail collection | Only read `/proc/[pid]/cmdline`, `/proc/[pid]/status` (user info) for processes that are actually visible on screen, not all processes | MEDIUM | 10-30% reduction in Proc::collect for systems with many processes | Currently reads `comm`, `cmdline`, and `status` for every new process. If 500 processes exist but only 30 are visible, 470 command-line reads are wasted. Cache and only fetch for visible + recently-seen PIDs |

### Anti-Features (Commonly Considered, Often Problematic)

Optimizations that seem appealing but create problems disproportionate to their benefit. These are traps.

| Feature | Why Appealing | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| SIMD-accelerated terminal rendering | Sounds impressive; SIMD is fast | Terminal output is string-based with variable-width UTF-8 and escape sequences. SIMD operates on fixed-width aligned data. The mismatch means complex shuffle/mask logic that's slower than scalar code for this workload. The bottleneck is I/O to the terminal, not string processing | Focus on reducing output volume (differential updates) rather than accelerating string building |
| Memory-mapped `/proc` files | mmap avoids read syscall overhead | `/proc` files are virtual -- they're generated on read. mmap doesn't help because there's no file to map into memory; the kernel still generates content on page fault. Also adds TLB pressure and requires `munmap` cleanup | Use raw `read()` into stack buffers -- minimal overhead, no cleanup needed |
| Custom allocator for all `std::string` | Eliminates malloc overhead globally | Massive code change touching every string in the codebase. Debugging becomes harder. SSO (Small String Optimization) already handles most short strings (< 15-22 chars depending on implementation). The real fix is avoiding unnecessary string creation, not making creation cheaper | Target specific hot-path strings with stack buffers or `string_view`; let SSO handle the rest |
| Rewriting draw functions in a "retained mode" UI framework | Eliminates redundant draws by tracking a widget tree | Architectural rewrite of 2500+ lines of draw code. The current immediate-mode approach is simpler to reason about. Incremental/differential output achieves most of the same benefit without a rewrite | Implement per-cell dirty tracking on top of existing draw functions |
| Lock-free data structures for collector-to-drawer communication | Eliminates mutex contention between collector and drawer threads | btop uses a single runner thread that collects then draws sequentially -- there's no contention to eliminate. The `atomic_lock` pattern is already lightweight. Lock-free structures add complexity for zero benefit in this architecture | The current `atomic<bool>` + sequential collect-then-draw is already optimal for the single-threaded runner model |
| Replacing `fmt` library with raw `sprintf` | Marginally faster for simple format strings | `fmt` is already close to optimal, provides type safety, and the project depends on it throughout. `sprintf` is not type-safe, can overflow, and modern `fmt` with `format_to` is comparable in speed | Use `fmt::format_to` with pre-allocated buffers instead of `fmt::format` for hot paths |
| Reducing update frequency to save CPU | Trivially reduces CPU usage | Violates the project constraint of "no changes to user-visible behavior." The update frequency is user-configurable and should remain so | Optimize what happens during each update, not how often updates occur |
| Aggressive `constexpr` everything | Sounds like free performance | Many values depend on runtime state (terminal size, config, theme). Over-applying `constexpr` creates maintenance burden with no runtime benefit. Only escape sequence constants and lookup tables benefit | Apply `constexpr` selectively to genuinely compile-time-known values: escape codes, symbol tables, numeric constants |

## Feature Dependencies

```
[Raw /proc read() with stack buffers]
    +-- enables --> [from_chars() number parsing]  (parse directly from read buffer)
    +-- enables --> [Lazy proc detail collection]   (cheap to skip reads when using raw fd)

[Enum-indexed arrays replacing unordered_map]
    +-- enables --> [Ring buffer for time-series]    (array<RingBuffer, N> instead of map<string, deque>)

[Differential terminal output]
    +-- requires --> [Per-cell state tracking]       (need to know what changed)
    +-- conflicts --> [Parallel data collection]     (harder to diff when data arrives async)

[Pre-computed escape sequences]
    +-- enhances --> [Differential terminal output]  (cached escapes = faster partial redraws)

[String copy elimination]
    +-- independent (can be done anytime)

[std::regex replacement]
    +-- independent (can be done anytime)
    +-- enhances --> [Process filter matching]       (faster filter = snappier UX on filter changes)

[Parallel data collection]
    +-- conflicts --> [Differential terminal output] (complicates change tracking)
    +-- requires --> [Thread-safe data structures]   (shared state protection)
```

### Dependency Notes

- **Raw /proc reads enable from_chars parsing:** When you have a `char[]` buffer from `read()`, you can parse numbers in-place with `from_chars()`. With `ifstream`, you're extracting into `string` first, then converting -- an unnecessary intermediate step.
- **Enum-indexed arrays enable ring buffers:** Once you replace `unordered_map<string, deque<long long>>` with `array<..., N>`, switching the `deque` to a ring buffer is a localized change per array slot.
- **Differential output conflicts with parallel collection:** If data arrives from multiple threads at unpredictable times, tracking "what changed since last frame" becomes much harder. Recommend doing differential output first, parallel collection later (or never -- wall-clock gains may not justify complexity).
- **String copy elimination is independent:** Can be done as a cleanup pass at any time. No dependencies on other optimizations.
- **std::regex replacement is independent:** Self-contained change. The `Fx::uncolor` regex is used in the final output path but doesn't interact with other optimizations.

## MVP Definition (First Optimization Phase)

### Phase 1: Profile and Measure Baseline (v0)

Establish measurable baselines before changing anything.

- [ ] Instrument with `perf` / Instruments.app to identify actual CPU hotspots
- [ ] Measure baseline: CPU%, RSS, startup time, time-per-frame
- [ ] Validate that Proc::collect and draw functions are indeed the top consumers (expected from code analysis but must be confirmed)

### Phase 2: Low-Hanging Fruit (v1)

High-impact, low-risk changes. Each is independently testable.

- [ ] Replace `std::regex` usage in `Fx::uncolor` and proc filter -- because it's on the critical output path every frame
- [ ] Eliminate string-by-value copies in `uresize`, `luresize`, `ljust`, `rjust`, `cjust` -- because they're called thousands of times per frame
- [ ] Replace `readfile()` with direct `read()` for small sysfs files -- because it eliminates 2 extra syscalls per call
- [ ] Use `from_chars()` instead of `sto*()` in proc stat parsing -- because it's zero-allocation and 2-5x faster
- [ ] Add `string::reserve()` to draw functions and runner output -- because it prevents reallocation cascades

### Phase 3: Architectural Wins (v1.x)

Changes that require more careful design but yield large gains.

- [ ] Raw `/proc` reads with stack buffers for Proc::collect -- trigger: after Phase 2 confirms proc collection is still the top hotspot
- [ ] Enum-indexed arrays replacing `unordered_map<string, ...>` in data structures -- trigger: after profiling shows map lookup overhead
- [ ] Ring buffers replacing `deque<long long>` for time-series data -- trigger: can be done alongside enum migration

### Phase 4: Advanced Optimizations (v2+)

Larger changes to defer until core optimizations prove insufficient.

- [ ] Differential terminal output -- requires careful design of change-tracking; biggest potential single win but highest complexity
- [ ] Parallel data collection -- defer because wall-clock improvement comes at the cost of increased total CPU; only worthwhile if single-core utilization is maxed
- [ ] `constexpr` escape sequences and compile-time symbol tables -- low impact individually; batch as a cleanup phase

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Expected Impact | Priority |
|---------|------------|---------------------|-----------------|----------|
| Profile + baseline measurement | HIGH | LOW | Foundation for all work | P0 |
| Replace `std::regex` in output path | HIGH | LOW | 5-15% CPU | P1 |
| Raw `/proc` reads with stack buffers | HIGH | MEDIUM | 20-40% proc collect | P1 |
| Eliminate string copies (uresize etc.) | HIGH | LOW | 3-8% CPU | P1 |
| `from_chars()` number parsing | MEDIUM | LOW | 3-8% proc collect | P1 |
| `reserve()` in draw functions | MEDIUM | LOW | 2-5% draw time | P1 |
| Replace `readfile()` with raw read | MEDIUM | LOW | 5-10% sensor/battery | P1 |
| Enum-indexed arrays for data structs | HIGH | MEDIUM | 10-20% overall | P2 |
| Ring buffers for time-series | MEDIUM | MEDIUM | 5-15% graph data | P2 |
| `fmt::format_to` consistency | LOW | LOW | 2-4% draw | P2 |
| Pre-computed escape sequences | LOW | LOW | 1-3% draw | P2 |
| Differential terminal output | HIGH | HIGH | 30-50% draw + I/O | P2 |
| Lazy proc detail collection | MEDIUM | MEDIUM | 10-30% proc collect | P2 |
| Parallel data collection | MEDIUM | HIGH | 20-40% wall-clock | P3 |
| `constexpr` escape sequences | LOW | LOW | 1-2% startup | P3 |
| Graph symbol flat array | LOW | LOW | 1-3% draw | P3 |

**Priority key:**
- P0: Must do before any optimization work
- P1: First optimization phase -- high impact, low risk
- P2: Second phase -- requires more design, still significant impact
- P3: Defer -- nice to have, lower ROI or higher risk

## Competitor Feature Analysis (Optimization Approaches)

| Optimization Area | htop | glances | bottom (Rust) | btop++ Current | btop++ Optimized |
|-------------------|------|---------|---------------|----------------|------------------|
| /proc parsing | C with raw read() | Python (slow) | Rust with BufReader | C++ ifstream | Raw read() + from_chars() |
| String handling | C char arrays (zero-copy) | Python strings | Rust String/&str | std::string with copies | string_view + reserve |
| Data structures | Simple arrays | Python dicts | Rust Vec + HashMap | unordered_map<string,...> | enum-indexed arrays |
| Terminal output | ncurses (diff-based) | curses (diff-based) | tui-rs (diff-based) | Full rebuild each frame | Differential (planned) |
| Regex | Not used | Python re | Rust regex (compiled) | std::regex (slow) | Manual parsing / CTRE |
| Memory model | Flat C structs | Python objects | Rust ownership | std::string in structs | Arena/pool (planned) |

## Sources

- Codebase analysis: `/Users/mit/Documents/GitHub/btop/src/` -- all `.cpp` and `.hpp` files examined
- [String Concatenation - CPP Optimizations Diary](https://cpp-optimizations.netlify.app/strings_concatenation/) -- benchmark data for string append vs fmt::format
- [CTRE Documentation](https://compile-time-regular-expressions.readthedocs.io/en/latest/) -- compile-time regex alternative to std::regex
- [libc++ std::regex performance issue](https://github.com/llvm/llvm-project/issues/60991) -- documents 10x slowness of std::regex on libc++
- [Daniel Lemire: Which is fastest: read, fread, ifstream?](https://lemire.me/blog/2012/06/26/which-is-fastest-read-fread-ifstream-or-mmap/) -- I/O method benchmarks
- [fmt library string concatenation issue](https://github.com/fmtlib/fmt/issues/1685) -- fmt::format_to vs fmt::format performance
- [clang-tidy performance-inefficient-string-concatenation](https://clang.llvm.org/extra/clang-tidy/checks/performance/inefficient-string-concatenation.html) -- automated detection of string copy issues

---
*Feature research for: C++ terminal system monitor performance optimization*
*Researched: 2026-02-27*
