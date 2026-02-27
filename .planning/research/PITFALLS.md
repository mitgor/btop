# Pitfalls Research

**Domain:** C++ terminal system monitor performance optimization (btop++)
**Researched:** 2026-02-27
**Confidence:** HIGH -- based on codebase analysis, C++ performance literature, and TUI optimization best practices

## Critical Pitfalls

### Pitfall 1: Optimizing Without Profiling Data (Guessing at Bottlenecks)

**What goes wrong:**
Developers assume they know what is slow -- typically fixating on micro-optimizations like loop unrolling or branch prediction -- while the real bottleneck is elsewhere entirely. In btop's case, the actual hot path is likely the per-PID `/proc` iteration (which opens 3-4 files per process via `ifstream` for hundreds of processes every update cycle) and the draw functions that build multi-kilobyte output strings via 162+ `+=` / `.append()` operations per frame. Optimizing the wrong thing wastes weeks and produces no measurable improvement.

**Why it happens:**
Developers trust intuition over data. C++ makes everything *look* potentially slow (templates, virtual dispatch, allocations), so developers scatter-optimize instead of focusing on measured hotspots. btop already has `DebugTimer` infrastructure but it measures wall-clock time per subsystem, not instruction-level hotspots.

**How to avoid:**
- Profile with `perf record` / `perf report` (Linux), Instruments (macOS) before writing any optimization code
- Use `perf stat` to measure IPC (instructions per cycle), cache miss rates, and branch misprediction rates
- Establish a reproducible benchmark: fixed terminal size, fixed number of processes, fixed update interval
- Set quantitative targets (e.g., "reduce CPU::collect from 8ms to 4ms") before starting work
- Re-profile after every change to confirm improvement

**Warning signs:**
- "I think this is slow because..." without profiling data
- Optimizing code that runs once per frame when the real cost is per-process-per-frame
- Spending time on code paths that profile data shows are <1% of total runtime
- Changes that show no measurable improvement in end-to-end benchmarks

**Phase to address:**
Phase 1 (Profiling & Benchmarking Setup). This must be the first phase -- all subsequent optimization phases depend on its output.

---

### Pitfall 2: Death by String Allocation in the Render Path

**What goes wrong:**
btop's draw functions (`Cpu::draw`, `Mem::draw`, `Net::draw`, `Proc::draw` in `btop_draw.cpp`) build the entire frame output by concatenating strings with `out += ...` -- 162+ concatenation operations in `btop_draw.cpp` alone. Each `+=` may trigger a reallocation and copy of the entire accumulated string. The `Mv::to()`, `Mv::r()`, `Mv::d()` cursor functions in `btop_tools.hpp` each call `to_string()` (98 calls across the codebase) which allocates. The `fmt::format()` calls (88 in `btop_draw.cpp`) also allocate intermediate strings. The final `output` string in the runner loop concatenates all box outputs together. This pattern creates hundreds of heap allocations per frame.

**Why it happens:**
String concatenation is the most natural way to build terminal output in C++. The code reads clearly and works correctly. The cost is invisible -- each `+=` is O(1) amortized for a single string, but the cumulative reallocation cost across hundreds of operations per frame becomes significant. The existing `out.reserve(width * height)` calls help but underestimate the true size because they do not account for escape code bytes.

**How to avoid:**
- Measure allocation count per frame using `perf record -e malloc` or a custom allocator wrapper before optimizing
- Pre-calculate accurate buffer sizes including escape code overhead (each color escape is 10-20 bytes)
- Use `reserve()` with accurate estimates rather than `width * height` (actual output includes escape codes, cursor movements, and formatting that typically 3-5x the visible character count)
- Consider `fmt::format_to(std::back_inserter(buffer), ...)` to append directly into a pre-allocated buffer instead of creating intermediate strings
- Convert `Mv::to()`, `Mv::r()`, `Mv::d()` to write directly into an output buffer rather than returning new strings
- Profile before and after to confirm allocations actually decreased

**Warning signs:**
- `perf` shows significant time in `malloc`/`free`/`operator new`/`operator delete`
- Frame render time scales non-linearly with terminal size
- Heap profiler shows hundreds of small allocations (20-200 bytes) per frame

**Phase to address:**
Phase 2 or 3 (String/Allocation Optimization). Must come after profiling confirms this is actually a bottleneck. Do not assume -- measure first.

---

### Pitfall 3: std::regex on the Hot Path

**What goes wrong:**
`std::regex` is notoriously slow across all major C++ standard library implementations. libc++ is approximately 10x slower than libstdc++, and even libstdc++ is slow compared to alternatives. btop uses `std::regex` in several places:
- `Fx::escape_regex` and `Fx::color_regex` are compiled as global `const` objects (good -- compiled once) but `Fx::uncolor()` calls `std::regex_replace` which is called in the runner loop output path (`btop.cpp:730`) when an overlay is active
- Process filtering via `std::regex_search` / `std::regex_match` in `btop_shared.cpp:183-185` runs against every visible process when a regex filter is active
- `Fx::uncolor()` is called to strip all color codes from the entire frame output -- a multi-kilobyte string -- every frame when the overlay is visible

Replacing these with hand-written parsers or a faster regex library could yield significant speedups in these code paths.

**Why it happens:**
`std::regex` is the standard library solution and appears correct and simple. Its poor performance is a well-documented issue in the C++ community but not obvious to developers who have not benchmarked it. The regex patterns used (`\033\[\d+;\d?;\d*;\d*;\d*(m){1}`) are simple enough that manual parsing would be straightforward and dramatically faster.

**How to avoid:**
- Profile to confirm regex is actually a bottleneck before replacing it
- For `Fx::uncolor()`: write a simple state machine that scans for `\033[` and skips to the next `m` -- this is trivial for ANSI escape codes and will be 10-100x faster than regex
- For process filtering: `std::regex` is user-facing and harder to replace, but consider compiling the regex once and reusing it (already done), and consider `std::string::find` for non-regex filters (already done -- regex is only used when filter starts with `!`)
- If a regex library is needed, Google RE2 is fast, safe, and thread-friendly, but adding it as a dependency conflicts with the project constraint of no new heavy dependencies

**Warning signs:**
- `perf` shows time in `std::regex_search`, `std::regex_replace`, or `std::regex_match`
- Frame render time spikes when overlay is active (due to `Fx::uncolor` on full output)
- Process list rendering slows when a regex filter is set with many processes

**Phase to address:**
Phase 2 (Hot Path Optimization) for `Fx::uncolor()`. Phase 3 or later for process filter regex, since it only applies when user has set a regex filter.

---

### Pitfall 4: Filesystem Syscall Storm in Process Collection

**What goes wrong:**
The Linux process collection loop (`linux/btop_collect.cpp:3002`) iterates over every PID directory in `/proc` and opens 3-4 files per process (`comm`, `cmdline`, `status`, `stat`) using `std::ifstream`. On a system with 500 processes, this means 1500-2000 `open()`/`read()`/`close()` syscall triplets per update cycle. Each `ifstream` construction also involves C++ runtime overhead (locale, stream state initialization). The `readfile()` utility function (`btop_tools.cpp:561`) additionally calls `fs::exists()` before opening -- adding an extra `stat()` syscall for every call. The macOS and FreeBSD collectors have similar patterns with their respective APIs.

**Why it happens:**
`/proc` is the standard Linux interface for process information. Reading individual files is the documented and correct approach. The overhead is per-syscall, not per-byte, so the bottleneck is the number of file operations rather than the amount of data read. `ifstream` is the idiomatic C++ approach but carries more overhead than raw `open()`/`read()`/`close()`.

**How to avoid:**
- Profile first -- measure how much time is actually spent in syscalls vs. parsing vs. string operations
- Consider using raw `open()`/`read()`/`close()` with stack-allocated buffers instead of `ifstream` for hot-path `/proc` reads -- this eliminates C++ stream initialization overhead
- Remove the redundant `fs::exists()` check in `readfile()` -- just try to open the file and handle failure
- Combine reads where possible: `/proc/[pid]/stat` already contains most needed fields; avoid opening `comm` and `status` separately for cached processes
- Cache file descriptors for `/proc/stat` and other global files that are read every cycle (use `lseek` + `read` instead of `close` + `open`)
- Consider reading `/proc/[pid]/stat` with a single `read()` into a stack buffer and parsing in-place rather than using `getline()` with heap-allocated strings

**Warning signs:**
- `strace -c` shows thousands of `openat()`/`read()`/`close()` syscalls per update
- CPU time in `Proc::collect` dominates total collection time
- Collection time scales linearly with process count (expected, but the constant factor matters)
- `perf` shows significant time in kernel `vfs_read`, `proc_pid_read_map`, or related proc filesystem functions

**Phase to address:**
Phase 2 or 3 (Data Collection Optimization). This is likely the single largest optimization opportunity on Linux, but must be confirmed by profiling.

---

### Pitfall 5: Breaking Cross-Platform Behavior While Optimizing

**What goes wrong:**
An optimization that works brilliantly on Linux (e.g., replacing `ifstream` with raw `read()` on `/proc`) has no equivalent on macOS (which uses `sysctl`, `proc_pidinfo`, IOKit) or FreeBSD (which uses `kvm_getprocs`, `sysctl`). Developers optimize the Linux path, test on Linux, and break macOS/FreeBSD builds or introduce behavioral differences. The platform-specific collectors (`linux/btop_collect.cpp`, `osx/btop_collect.cpp`, `freebsd/btop_collect.cpp`) share the same interface but have entirely different implementations -- an optimization strategy that assumes Linux internals will not transfer.

**Why it happens:**
Developers typically develop and test on one platform. Linux is the most common development target and has the most optimization opportunities (procfs is well-understood). macOS and FreeBSD have different performance characteristics and bottlenecks. The shared header (`btop_shared.hpp`) defines the interface but the implementations diverge significantly.

**How to avoid:**
- Test on all three platforms (Linux, macOS, FreeBSD) before merging any optimization, or at minimum ensure CI covers all platforms
- Keep optimizations in platform-specific files when they involve platform-specific APIs
- Shared-code optimizations (string handling, draw path, data structures) are safer -- they benefit all platforms equally
- When optimizing the Linux collector, identify the *equivalent* optimization for macOS/FreeBSD rather than leaving those platforms unoptimized
- Use `#ifdef` blocks only for platform-specific code; prefer portable C++ for shared logic

**Warning signs:**
- Optimization only touches `linux/btop_collect.cpp` without considering `osx/` and `freebsd/`
- CI builds fail on non-Linux platforms after optimization changes
- Performance improves on Linux but regresses or stays flat on macOS
- New platform-specific code paths without corresponding test coverage

**Phase to address:**
Every phase. This is a cross-cutting concern. Each optimization must be validated on all supported platforms.

---

### Pitfall 6: Introducing Data Races in the Collect/Draw Pipeline

**What goes wrong:**
btop uses a secondary runner thread (`Runner::_runner` in `btop.cpp`) that runs collect and draw functions, while the main thread handles input and signals. These threads communicate through `atomic<bool>` flags (`Runner::active`, `Runner::stopping`, `Runner::redraw`, `Global::resized`). Global mutable state is pervasive: `Cpu::box`, `Mem::box`, `Net::box`, `Proc::box` are all global strings; `current_procs` is a global vector. The `atomic_lock` class provides coarse-grained synchronization, but optimizations that introduce new shared state or change access patterns can easily create data races. For example, adding a cache that is written by the collect thread and read by the draw thread without synchronization.

**Why it happens:**
Performance optimizations often involve caching, buffering, or parallelizing work -- all of which introduce shared mutable state. The existing code has carefully-placed atomic operations, but the synchronization model is implicit (not documented) and relies on the runner thread being the only thread that accesses most data structures. Any optimization that breaks this implicit contract creates subtle, intermittent bugs.

**How to avoid:**
- Understand the existing threading model before modifying it: the runner thread owns all collect/draw data; the main thread owns input; communication is via atomic flags
- Do not introduce new shared mutable state without explicit synchronization
- Use ThreadSanitizer (`-fsanitize=thread`) in CI and during development
- Prefer immutable data or thread-local storage for optimization caches
- If parallelizing collection (e.g., collecting CPU and MEM simultaneously), ensure no shared mutable state between collectors
- Document any new synchronization requirements

**Warning signs:**
- Intermittent crashes or incorrect data that cannot be reliably reproduced
- Valgrind/TSan reports data race warnings
- Optimization works in single-threaded testing but fails under real operation
- New `static` variables in functions called from the runner thread

**Phase to address:**
Every phase, but especially Phase 3+ when more aggressive optimizations are attempted (parallelization, caching, buffer reuse).

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Platform-specific optimization without portable fallback | Faster results on one OS | Maintenance burden, divergent behavior across platforms | Only in platform-specific files (`linux/`, `osx/`, `freebsd/`) with equivalent work planned for other platforms |
| Replacing `std::string` with raw `char*` buffers | Eliminates allocation overhead | Loses memory safety, introduces buffer overflow risk, harder to maintain | Never in shared code; acceptable in hot-path platform collectors with careful bounds checking |
| Adding compiler-specific intrinsics (`__builtin_expect`, SIMD) | Measurable speedup on GCC/Clang | Breaks portability to other compilers, obscures intent | Only behind `#ifdef` with standard fallback; only when profiling shows >5% improvement |
| Removing safety checks (bounds checking, null checks) for speed | Avoids branch overhead | Crashes in edge cases (zombie processes, /proc race conditions, unexpected kernel changes) | Never -- `/proc` data is inherently unreliable; safety checks are mandatory |
| Global pre-allocated buffers to avoid per-frame allocation | Zero allocation per frame | Thread safety issues, maximum size assumptions, memory waste when idle | Acceptable if thread-confined and sized to reasonable maximums with overflow fallback |
| Skipping `shrink_to_fit()` calls to avoid reallocation | Avoids one reallocation | Unbounded memory growth for containers that spike in size | Acceptable for containers with bounded maximum size; review memory profile periodically |

## Performance Traps

Patterns that work at small scale but fail under stress.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Linear scan for PID lookup (`rng::find(current_procs, pid)` at line 3020) | Collection time grows quadratically with process count | Use a hash map (`unordered_map<size_t, size_t>`) for PID-to-index lookup | >500 processes (O(n) per PID * n PIDs = O(n^2) total) |
| `deque<long long>` for time series data (used throughout for CPU/memory/network history) | Cache misses during graph rendering due to non-contiguous memory | Profile cache miss rate; consider circular buffer over `vector` if iteration dominates | When graph width exceeds deque chunk size (typically 512 bytes / 8 bytes = 64 elements) |
| `unordered_map<string, ...>` for fixed small key sets (e.g., `cpu_percent` with 11 known keys) | Hash computation + heap allocation for every lookup by string key | Use `enum` keys with `array` or `flat_map` for fixed key sets; eliminates hashing and allocation | Always -- overhead is constant but unnecessary for known-at-compile-time keys |
| Building full output string then calling `Fx::uncolor()` with regex on entire output | Frame time spikes proportional to output size when overlay is active | Track colored/uncolored output in parallel, or strip inline during construction | Terminal sizes >100x40 with overlay active |
| `std::filesystem::directory_iterator` for `/proc` enumeration | Allocates per entry, slower than raw `opendir`/`readdir` | Profile; consider `opendir`/`readdir` if allocation overhead is significant | Systems with >1000 processes |
| `fmt::format()` creating intermediate strings for cursor movement | Hundreds of small allocations per frame for escape sequences | Use `fmt::format_to()` with back_inserter, or pre-computed escape code tables | Every frame -- constant overhead but multiplied by number of UI elements |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Profiling setup:** Often missing reproducible benchmark conditions -- verify fixed terminal size, fixed process count, fixed update interval, CPU frequency governor set to `performance`
- [ ] **String optimization:** Often missing escape code size accounting -- verify `reserve()` calls include 3-5x overhead for ANSI escape codes, not just visible character count
- [ ] **Allocation reduction:** Often missing measurement of *actual* allocation count -- verify with `perf record -e probe_libc:malloc` or custom allocator, not just wall-clock time
- [ ] **Cache optimization:** Often missing cache invalidation -- verify cached data is properly refreshed when terminal is resized, theme changes, or config is reloaded
- [ ] **Cross-platform testing:** Often missing FreeBSD/NetBSD/OpenBSD -- verify builds and runs on all five platform-specific collector implementations, not just Linux and macOS
- [ ] **Thread safety:** Often missing TSan validation -- verify no data races with `clang++ -fsanitize=thread` under real operation (not just unit tests)
- [ ] **Regression testing:** Often missing visual correctness -- verify output is byte-identical before and after optimization (capture terminal output to file and diff)
- [ ] **Memory usage measurement:** Often missing RSS tracking -- verify memory usage did not increase as a side effect of performance optimization (pre-allocated buffers consume resident memory)

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Optimized wrong bottleneck | LOW | Revert changes, re-profile, redirect effort to actual bottleneck. No harm done if changes were isolated. |
| String allocation regression (slower after "optimization") | LOW | Revert, benchmark with allocation profiler, apply targeted fix only where allocations are confirmed expensive. |
| Cross-platform build break | MEDIUM | Fix immediately; add platform-specific CI checks; review optimization for portability. May need platform-specific implementation paths. |
| Data race introduced | HIGH | Extremely difficult to debug. Run TSan to identify. May require rethinking optimization approach. Revert to last known-good state and re-approach with explicit synchronization design. |
| Visual regression (incorrect rendering) | MEDIUM | Capture reference output before optimization. Diff against post-optimization output. Regression is usually in escape code sequencing or string truncation. |
| Memory growth from pre-allocated buffers | LOW | Add monitoring for resident memory; cap buffer sizes; add `shrink_to_fit()` on config change or terminal resize. |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Optimizing without profiling | Phase 1: Profiling & Benchmarking | Profiling infrastructure exists; baseline measurements documented; no optimization code written without profile data |
| String allocation in render path | Phase 2: String/Allocation Optimization | Allocation count per frame measured before and after; frame render time decreased |
| std::regex on hot path | Phase 2: Hot Path Optimization | `Fx::uncolor()` replaced with manual parser; profile confirms regex is no longer in hot path |
| Filesystem syscall storm | Phase 3: Data Collection Optimization | `strace -c` shows reduced syscall count; `Proc::collect` time decreased |
| Cross-platform breakage | Every phase (CI enforcement) | CI builds pass on Linux, macOS, FreeBSD for every change |
| Data races | Every phase (TSan enforcement) | TSan runs clean; no new global mutable state without synchronization |
| Premature micro-optimization | Every phase (review discipline) | Every optimization PR includes before/after profile data and benchmark results |

## Sources

- [C++ Performance Optimization: Avoiding Common Pitfalls](https://medium.com/@threehappyer/c-performance-optimization-avoiding-common-pitfalls-and-best-practices-guide-81eee8e51467) -- MEDIUM confidence
- [10 C++ Mistakes That Secretly Kill Your Performance](https://medium.com/codetodeploy/10-c-mistakes-that-secretly-kill-your-performance-i-was-guilty-of-4-for-years-9b4a6c5bf425) -- MEDIUM confidence
- [String concatenation optimization patterns](https://cpp-optimizations.netlify.app/strings_concatenation/) -- HIGH confidence (established reference)
- [clang-tidy performance-inefficient-string-concatenation](https://clang.llvm.org/extra/clang-tidy/checks/performance/inefficient-string-concatenation.html) -- HIGH confidence (official LLVM docs)
- [libc++ std::regex 10x slower than libstdc++](https://github.com/llvm/llvm-project/issues/60991) -- HIGH confidence (LLVM bug tracker)
- [Abseil Performance Tip: Regex Efficiency](https://abseil.io/fast/21) -- HIGH confidence (Google Abseil)
- [How Fast is Procfs](https://avagin.github.io/how-fast-is-procfs.html) -- MEDIUM confidence (kernel developer blog)
- [Algorithms for High Performance Terminal Apps (Textual)](https://textual.textualize.io/blog/2024/12/12/algorithms-for-high-performance-terminal-apps/) -- MEDIUM confidence (TUI framework author)
- [flat_map & flat_set: Your New Performance Toolkit](https://medium.com/@sofiasondh/flat-map-flat-set-your-new-performance-toolkit-43fee046a08b) -- MEDIUM confidence
- [Effortless Performance Improvements: std::unordered_map](https://julien.jorge.st/posts/en/effortless-performance-improvements-in-cpp-std-unordered_map/) -- MEDIUM confidence
- [Agner Fog: Optimizing Software in C++](https://www.agner.org/optimize/optimizing_cpp.pdf) -- HIGH confidence (authoritative reference)
- [Pitfalls of Premature Optimization](https://eshman.pro/2025/03/the-pitfalls-of-premature-code-optimization-lessons-from-experience/) -- MEDIUM confidence
- [Profile-Guided Optimization (PGO) research](https://arxiv.org/html/2507.16649v1) -- HIGH confidence (academic paper)
- btop++ source code analysis (direct codebase inspection) -- HIGH confidence

---
*Pitfalls research for: C++ terminal system monitor performance optimization (btop++)*
*Researched: 2026-02-27*
