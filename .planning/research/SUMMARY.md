# Project Research Summary

**Project:** btop++ performance optimization
**Domain:** C++ terminal system monitor — CPU, memory, process collection, and rendering pipeline optimization
**Researched:** 2026-02-27
**Confidence:** HIGH

## Executive Summary

btop++ is a mature C++23 terminal system monitor with a clean two-thread architecture: a main thread that handles input and signals, and a runner thread that sequences data collection followed by draw-to-string for each box (CPU, memory, network, process, GPU), then flushes a single concatenated output string to the terminal. The codebase is architecturally sound and logically organized, but it carries a set of well-understood C++ performance antipatterns that accumulate into meaningful overhead on every refresh cycle. The dominant costs are the Linux process collection loop (which opens 3-4 files per PID via `std::ifstream` — up to 2,000+ heap-allocated stream objects per cycle on busy systems) and the rendering pipeline (which builds tens of kilobytes of escape-sequence strings via hundreds of small string concatenations and `fmt::format()` calls that each allocate intermediate strings).

The recommended optimization approach is sequential and evidence-driven: profile first to quantify the actual hotspots, then attack them in order of impact and risk. The research is consistent across all four areas: profiling infrastructure must precede code changes, string/allocation reduction in both the draw path and process collection is the highest-ROI first phase, data structure improvements (replacing `unordered_map<string,...>` and `deque` with enum-indexed arrays and ring buffers) are the structural second phase, and advanced work like differential terminal output and parallel collection are deferred unless profiling after earlier phases shows they are still warranted. The full compiler-level optimization (PGO) should be the final phase, applied after code-level improvements are stable.

The primary risks are: (1) optimizing the wrong thing without profiling data — mitigated by mandating a measurement-first protocol; (2) introducing data races when caching or parallelizing — mitigated by running ThreadSanitizer continuously; and (3) breaking cross-platform behavior (Linux optimizations using `/proc` do not apply to macOS or FreeBSD collectors) — mitigated by CI on all three platforms for every change. None of these risks are novel; all have documented prevention strategies.

## Key Findings

### Recommended Stack

The profiling stack is well-established and fully available. Linux is the primary target: `perf` + Hotspot (KDAB) for CPU sampling, `heaptrack` + `heaptrack_gui` for allocation profiling, Valgrind Cachegrind + KCachegrind for cache miss analysis, and `strace` for syscall counting. On macOS, Instruments (via `xcrun xctrace`) is the only viable hardware-level profiler — Valgrind is deprecated on ARM64. FreeBSD uses `pmcstat` + FlameGraph scripts. For micro-benchmarking individual changes, `nanobench` (single-header, no dependencies) is the correct choice. All profiling builds should use `RelWithDebInfo` + `-fno-omit-frame-pointer`; never use `-O3` blindly as documented regressions exist in 2025 compiler versions. PGO (`-fprofile-generate`/`-fprofile-use`) is the highest-ROI compiler technique and should be applied last, after code-level optimizations are complete and stable. The project already uses `fmt` (header-only) and has LTO support in CMakeLists.txt — both should be leveraged.

**Core technologies:**
- `perf` + Hotspot: Linux CPU profiling — zero-copy kernel integration, <2% overhead, the standard
- Instruments (xctrace): macOS profiling — only hardware-level option, CLI-accessible
- `heaptrack` + `heaptrack_gui`: heap allocation profiling — much faster than Valgrind Massif, shows allocation hotspots
- Valgrind Cachegrind + KCachegrind: cache miss analysis — identifies data layout problems after perf finds hot functions
- `nanobench`: single-header micro-benchmarking — A/B tests individual optimizations, zero new dependencies
- `hyperfine`: startup and end-to-end benchmarking — statistical rigor, warmup support
- `mimalloc` (via `LD_PRELOAD`): drop-in allocator replacement — test without code changes if heaptrack shows high churn
- PGO (GCC/Clang): profile-guided optimization — 10-20% typical gain, apply last

### Expected Features (Optimizations)

**Must have — table stakes (P0/P1):**
- Profiling baseline established before any code changes — all other work depends on this
- Replace `std::regex` in `Fx::uncolor()` with a hand-written ANSI escape state machine — called per-frame when overlay is active; a commented-out manual parser already exists, disabled only due to a musl compatibility issue
- Eliminate pass-by-value string copies in `uresize`, `luresize`, `ljust`, `rjust`, `cjust` — called thousands of times per frame
- Replace `readfile()` + `ifstream` with raw `open()`/`read()` + stack buffers for `/proc` files — eliminates `fs::exists()` pre-check (extra `stat()`) and `ifstream` heap allocation per file
- Use `std::from_chars()` instead of `stoull`/`stoll`/`stoi` throughout proc stat parsing — zero-allocation, 2-5x faster
- Add accurate `string::reserve()` to draw functions — reserve must account for escape code overhead (3-5x visible character count), not just `width * height`
- Replace `fs::directory_iterator` with POSIX `opendir`/`readdir` for `/proc` enumeration

**Should have — architectural wins (P2):**
- Enum-indexed arrays replacing `unordered_map<string, deque<long long>>` in `cpu_info`, `mem_info`, `net_info` — eliminates per-lookup string hashing for fixed key sets
- Ring buffers (`std::array` with head/tail indices) replacing `deque<long long>` for time-series graph data — contiguous memory, zero allocation per data point
- Hash-map PID lookup replacing `rng::find` linear scan in `current_procs` — current pattern is O(N^2) for process collection on systems with many processes
- Consistent `fmt::format_to` with `back_inserter` replacing `fmt::format` in hot draw paths — eliminates intermediate string allocation
- Lazy proc detail collection (read `cmdline`/`status` only for visible processes, not all PIDs)
- Pre-computed static escape sequences for fixed positions (box corners, headers that only change on resize)
- Differential/incremental terminal output — only emit changed cells; biggest potential single win (30-50% draw reduction) but highest complexity

**Defer to v2+ (P3):**
- Parallel data collection across boxes — increases total CPU usage, conflicts with differential output, only worthwhile if single-core is maxed
- `constexpr` escape sequence generation — low individual impact, batch as cleanup
- Pool/arena allocation for `proc_info` strings — high engineering cost, lower ROI than structural changes above
- Tracy Profiler instrumentation — adds ongoing maintenance burden; btop is not a game engine

**Anti-features to avoid:**
- SIMD-accelerated terminal rendering — string/escape output doesn't fit SIMD data layout
- Memory-mapped `/proc` files — `/proc` is virtual; `mmap` triggers the same kernel generation on page fault
- Custom allocator for all `std::string` — massive scope, SSO already handles short strings; target specific allocations instead
- `std::regex` replacement with Google RE2 — adds a dependency; hand-written state machine is sufficient for the ANSI escape pattern

### Architecture Approach

btop++ uses a namespace-as-module pattern with no virtual dispatch and no dependency injection. The runner thread is the sole owner of all collection and draw data, which eliminates lock contention but serializes work that could theoretically be parallelized. The hot path is fully identified: `Proc::collect` on Linux (reads 3-4 `/proc/[pid]/` files per process via `ifstream`) and the draw pipeline in `btop_draw.cpp` (2,517 lines of string building) dominate. The architecture is well-suited to optimization: platform collectors are cleanly isolated in `linux/`, `osx/`, `freebsd/` directories, shared utilities in `btop_tools` are hot but self-contained, and the single-flush output pattern is already optimal for terminal I/O.

**Major components:**
1. **Runner thread** (`btop.cpp`, ~460L) — sequences collect-then-draw for each box; owns all mutable data; triggered via `binary_semaphore`
2. **Process collector** (`linux/btop_collect.cpp`, 3,341L) — highest-cost component; per-PID `/proc` file iteration with `ifstream` is the primary target
3. **Draw pipeline** (`btop_draw.cpp`, 2,517L) — second-highest cost; Graph, Meter, and per-box `draw()` functions that build multi-KB escape-sequence strings
4. **String utilities** (`btop_tools.cpp/hpp`, ~1,094L) — `ulen`, `uresize`, `ljust`, `rjust`, `floating_humanizer` called hundreds of times per frame; allocation-heavy
5. **Shared data structures** (`btop_shared.hpp`) — `cpu_info`, `mem_info`, `net_info`, `proc_info` definitions; changing these ripples to all collectors and draw functions
6. **Config** (`btop_config.cpp/hpp`) — `unordered_map`-backed with lock/unlock cycle; read frequently in draw functions; local caching per draw call partially mitigates this

### Critical Pitfalls

1. **Optimizing without profiling data** — The actual bottlenecks (Proc::collect I/O, draw string allocation) look likely from code review but must be confirmed by `perf` before writing any optimization code. Avoid; fix: establish baseline measurements first, require before/after profile data in every optimization change.

2. **String allocation death spiral in the render path** — 162+ `+=` operations in `btop_draw.cpp`, 98 `to_string()` calls for cursor movement escape sequences, 88 `fmt::format()` calls that each return new strings. The cumulative allocation count per frame is in the hundreds. Fix: accurate `reserve()`, `fmt::format_to` with `back_inserter`, reusable output buffers.

3. **`std::regex` on the hot path** — `Fx::uncolor()` runs `std::regex_replace` on the entire (multi-KB) output string every frame when an overlay is active. libc++ `std::regex` is documented at 10x slower than libstdc++. Fix: hand-written ANSI escape state machine; a disabled version already exists in `btop_tools.cpp:189-210`.

4. **Filesystem syscall storm in Proc::collect** — 1,500-2,000+ `open()`/`read()`/`close()` triplets per update cycle from `ifstream` per PID. The `readfile()` utility adds a `stat()` syscall via `fs::exists()` before every open. Fix: POSIX `open()`/`read()` with stack buffers; remove `fs::exists()` pre-check; cache fd for frequently-read global files like `/proc/stat`.

5. **Cross-platform breakage from Linux-specific optimizations** — `/proc`-based optimizations do not apply to macOS (which uses `sysctl`, `proc_pidinfo`, IOKit) or FreeBSD (`kvm_getprocs`, `sysctl`). Shared-code optimizations (string utilities, draw path, data structures) are safe on all platforms. Fix: CI on Linux, macOS, and FreeBSD for every change; platform-specific optimizations must stay in platform-specific directories with equivalent work planned for other platforms.

6. **Data races from caching or parallelizing** — The runner thread owns all data by implicit convention, not by enforcement. Adding caches or splitting the collection into parallel goroutines breaks this contract. Fix: ThreadSanitizer in CI; document any new synchronization; prefer thread-local or immutable structures for optimization caches.

## Implications for Roadmap

Based on research, the phase structure follows a strict dependency chain: measure first, fix cheap/safe things second, fix structural things third, then do advanced work only if earlier phases show it is still warranted.

### Phase 1: Profiling Infrastructure and Baseline

**Rationale:** No optimization should be written before the bottlenecks are measured. The research identifies likely hotspots (Proc::collect, draw string building) but "likely" is not sufficient. Every subsequent phase depends on this phase's output. This is the highest-priority risk mitigation from PITFALLS.md (Pitfall 1).

**Delivers:** Reproducible benchmark suite; documented baseline metrics (CPU% per box, time-per-frame, allocation count per frame, syscall count per update); confirmed hotspot ranking; CMake presets for `RelWithDebInfo` profiling builds.

**Addresses:** Profiling + baseline measurement (P0 from FEATURES.md)

**Uses:** `perf` + Hotspot (Linux), Instruments (macOS), `heaptrack`, `hyperfine`, `strace`

**Avoids:** Pitfall 1 (optimizing without data), Pitfall 5 (need cross-platform baseline on Linux, macOS, FreeBSD)

**Research flag:** Standard patterns — skip `/gsd:research-phase`. perf, Hotspot, heaptrack installation and usage are well-documented.

---

### Phase 2: String and Allocation Reduction (Low-Risk Quick Wins)

**Rationale:** The string utility functions (`uresize`, `ljust`, `rjust`, `cjust`) and draw path patterns (`Mv::to()`, `fmt::format`, `+=`) are independent leaf-level changes that touch no shared data structures, pose no threading risk, and benefit all platforms equally. They can be done in any order and benchmarked individually. The `std::regex` replacement in `Fx::uncolor()` is self-contained and the hand-written parser already exists in the codebase — just needs the musl compatibility fix.

**Delivers:** Measurable reduction in per-frame allocation count and draw time; regex replaced in output path; string utilities converted to `string_view` / pass-by-reference; `reserve()` calls with accurate size estimates; `fmt::format_to` used consistently.

**Addresses:** Replace `std::regex` (P1), eliminate string copies (P1), `reserve()` in draw functions (P1), `fmt::format_to` consistency (P2), pre-computed escape sequences (P2)

**Uses:** `nanobench` for A/B testing individual changes; `heaptrack` to verify allocation reduction; `perf` to confirm before/after

**Avoids:** Pitfall 2 (string allocation), Pitfall 3 (regex on hot path); cross-platform safe (all changes in shared code)

**Research flag:** Standard patterns — skip `/gsd:research-phase`. String optimization patterns are well-documented.

---

### Phase 3: File I/O Optimization (Proc::collect)

**Rationale:** The Linux process collection loop is the most likely single largest cost on systems with >100 processes. Replacing `ifstream` with POSIX `open()`/`read()`/stack buffers is well-understood, the change is confined to `linux/btop_collect.cpp`, and it enables the `from_chars()` parsing improvement as a natural follow-on. This phase must come after Phase 1 (to confirm Proc::collect is actually dominant) but is independent of Phase 2 (the two sets of changes don't interact). The O(N^2) linear PID lookup (`rng::find`) is also addressed here.

**Delivers:** POSIX I/O in Linux collector; `opendir`/`readdir` replacing `fs::directory_iterator`; hash-map PID lookup replacing linear scan; `from_chars()` replacing `stoull`/`stoll`; lazy proc detail collection for off-screen processes; `fs::exists()` removed from `readfile()`.

**Addresses:** Raw `/proc` reads (P1 high impact), `from_chars()` parsing (P1), `readfile()` optimization (P1), lazy proc detail (P2)

**Uses:** `strace -c` to verify syscall count reduction; `perf` to confirm Proc::collect time reduction

**Avoids:** Pitfall 4 (syscall storm), Pitfall 5 (Linux-only — equivalent investigation needed for macOS/FreeBSD collectors)

**Research flag:** May benefit from `/gsd:research-phase` for the macOS/FreeBSD equivalent patterns — `proc_pidinfo`, `sysctl KERN_PROC`, and `kvm_getprocs` have different performance characteristics than Linux `/proc`.

---

### Phase 4: Data Structure Modernization

**Rationale:** Replacing `unordered_map<string, deque<long long>>` with enum-indexed arrays and ring buffers touches `btop_shared.hpp`, which ripples to all collectors AND all draw functions. This is the riskiest structural change and must come after Phase 3 (collectors stabilized with new I/O patterns) and Phase 2 (draw path stabilized). The benefit compounds across the whole pipeline because both collect and draw read/write these structures every cycle.

**Delivers:** Enum-indexed arrays for `cpu_info`, `mem_info`, `net_info` time-series fields; ring buffers replacing `deque<long long>` for graph history; flat array replacing `unordered_map` for graph symbol lookup; struct fields or enum-indexed array for fixed-key Config lookups.

**Addresses:** Enum-indexed arrays (P2 high impact), ring buffers (P2 medium impact), graph symbol flat array (P3)

**Uses:** `perf` + Cachegrind to confirm cache behavior improvement; `nanobench` for before/after map lookup benchmarks

**Avoids:** Pitfall 5 (shared header — changes apply cross-platform by definition), Pitfall 6 (data structure changes must not introduce new shared mutable state)

**Research flag:** Standard patterns — skip `/gsd:research-phase`. Enum-indexed arrays and ring buffer patterns are well-documented in C++ performance literature.

---

### Phase 5: Draw Pipeline and Advanced Rendering

**Rationale:** The draw pipeline depends on stable data structures (Phase 4) and optimized string utilities (Phase 2). Differential terminal output is the single highest-potential optimization (30-50% draw reduction) but also the highest complexity. It should only be attempted after earlier phases show the remaining cost is in rendering, not collection. This phase also includes Graph class optimization and `Config::getX()` call reduction in draw functions.

**Delivers:** Differential/incremental terminal output (only redraw changed cells); Graph double-buffering overhead investigation; reduced `Config::get*()` lookups via local caching at start of draw functions; `writev()` scatter-gather output to avoid final string concatenation; `constexpr` escape sequence cleanup.

**Addresses:** Differential terminal output (P2 high impact), `constexpr` escape sequences (P3), batch terminal writes (P2)

**Uses:** `perf` + `strace` to measure terminal write reduction; visual regression testing (capture and diff terminal output before/after)

**Avoids:** Pitfall 2 (output string remains the key allocation point), Pitfall 6 (differential tracking requires careful state management — no shared mutable state between main and runner threads)

**Research flag:** Differential terminal output needs `/gsd:research-phase`. The pattern is used by ncurses, htop, and `tui-rs` but implementing it on top of btop's existing immediate-mode draw functions requires specific design work. Per-cell dirty tracking in a non-curses codebase is non-trivial.

---

### Phase 6: Compiler-Level Optimization and Verification

**Rationale:** PGO must be applied after code-level optimizations are complete — the profile data used to guide the compiler should reflect the optimized code, not the original. This phase also includes the full sanitizer sweep (ASan, TSan, UBSan) on the optimized build, final performance regression comparison against Phase 1 baseline, and optional mimalloc evaluation.

**Delivers:** PGO CMake presets (`pgo-generate`, `pgo-use`); benchmarked `-O2+PGO` vs `-O3+PGO` vs current baseline; mimalloc evaluation via `LD_PRELOAD`; sanitizer-clean optimized build; final performance report against Phase 1 baseline.

**Addresses:** PGO compiler optimization (high impact, apply last), LTO verification (already in CMakeLists.txt but verify it's active), mimalloc testing

**Uses:** GCC `-fprofile-generate`/`-fprofile-use` or Clang `-fprofile-instr-generate`/`-fprofile-instr-use`; ASan, TSan, UBSan; `hyperfine` for end-to-end timing

**Avoids:** Pitfall 1 (PGO without profiling data is useless; representative workload required), Pitfall 5 (PGO is cross-platform compatible)

**Research flag:** Standard patterns — skip `/gsd:research-phase`. GCC and Clang PGO workflows are well-documented in official docs.

---

### Phase Ordering Rationale

- **Phase 1 must be first:** Without baseline measurements, no other phase can demonstrate improvement. This is the single strongest finding across all four research files.
- **Phase 2 before Phase 5:** String utilities are leaf-level dependencies of the draw pipeline. Stabilize them before tackling the pipeline as a whole.
- **Phase 3 independent of Phase 2:** File I/O optimization in Proc::collect and string allocation reduction in the draw path have no interactions. They could theoretically run in parallel but sequential is safer for attribution.
- **Phase 4 after Phase 2 and 3:** Changing `btop_shared.hpp` data structures ripples everywhere. Do this after the collectors (Phase 3) and string utilities (Phase 2) are stable.
- **Phase 5 after Phase 4:** Draw pipeline optimization requires stable data structures and string utilities. Differential output is especially sensitive to data structure layout.
- **Phase 6 last:** PGO applied to already-optimized code gives better guidance to the compiler than PGO applied to the original code.

### Research Flags

Phases needing deeper research during planning:
- **Phase 3** (File I/O — macOS/FreeBSD equivalents): The macOS collector uses `proc_pidinfo`, `sysctl KERN_PROC_ALL`, and IOKit; the FreeBSD collector uses `kvm_getprocs`. The equivalent optimization strategies for these APIs are less documented than Linux `/proc` optimization and warrant a research-phase before implementation.
- **Phase 5** (Differential terminal output): Implementing per-cell dirty tracking on top of btop's immediate-mode draw functions is novel work with no directly applicable reference implementation. The approach used by ncurses and `tui-rs` is different enough that adaptation will require design research.

Phases with standard patterns (skip `/gsd:research-phase`):
- **Phase 1:** perf, Hotspot, heaptrack, Instruments usage are all well-documented with high-confidence references
- **Phase 2:** String optimization patterns (`string_view`, `reserve`, `fmt::format_to`, hand-written ANSI parser) are standard C++ performance techniques with official documentation
- **Phase 4:** Enum-indexed arrays and ring buffers are textbook data structure improvements with clear implementation patterns
- **Phase 6:** PGO workflow is documented in official GCC and LLVM/Clang docs

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All tools have official documentation; version requirements are verified. Platform-specific gaps on FreeBSD are MEDIUM (less community tooling). |
| Features | HIGH | Based on direct codebase analysis of all source files. Feature impact estimates (e.g., "20-40% proc collect reduction") are derived from known cost models, not measured — treat as directional targets to be confirmed by profiling. |
| Architecture | HIGH | Based on direct source code analysis of the btop++ repository. Component boundaries, data flow, and hot paths are identified from actual code, not inference. |
| Pitfalls | HIGH | Pitfalls 1-4 and 6 are confirmed by codebase analysis. Pitfall 5 (cross-platform) is structural and well-understood. One MEDIUM-confidence source in Pitfalls.md (Medium blog posts) but corroborated by official documentation. |

**Overall confidence:** HIGH

### Gaps to Address

- **Platform-specific collection optimization on macOS and FreeBSD:** The Linux optimization strategy is detailed (Proc::collect via `/proc`). The equivalent macOS and FreeBSD collector optimizations are less researched. When Phase 3 is planned, explicit research into `proc_pidinfo`, `sysctl KERN_PROC`, and `kvm_getprocs` performance characteristics is needed.

- **Impact estimates are pre-measurement:** All percentage estimates (e.g., "20-40% reduction in Proc::collect") are projections based on known cost models, not measured on this codebase. Phase 1 profiling will either confirm or revise these estimates. Do not commit to optimization targets until Phase 1 baseline data exists.

- **Differential output design:** The FEATURES.md and ARCHITECTURE.md both flag this as the highest-potential single optimization, but neither provides a concrete design for implementing per-cell dirty tracking on btop's existing draw functions. This needs dedicated design work in Phase 5 planning.

- **musl compatibility of the existing manual ANSI parser:** The commented-out manual ANSI escape parser in `btop_tools.cpp:189-210` was disabled "due to issue when compiling with musl." The specific issue is undocumented in the research. Before treating this as a trivial restoration, the musl compatibility problem needs investigation.

## Sources

### Primary (HIGH confidence)
- Direct btop++ source analysis — `src/btop.cpp`, `src/btop_draw.cpp`, `src/linux/btop_collect.cpp`, `src/btop_tools.cpp/hpp`, `src/btop_shared.hpp` — architecture, hotspots, data structures
- [Brendan Gregg's perf Examples](https://www.brendangregg.com/perf.html) — Linux perf profiling reference
- [KDAB Hotspot GitHub](https://github.com/KDAB/hotspot) — perf.data visualization
- [KDE heaptrack GitHub](https://github.com/KDE/heaptrack) — heap allocation profiling
- [LLVM PGO Documentation](https://llvm.org/docs/HowToBuildWithPGO.html) — Clang PGO workflow
- [GCC Optimization Options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html) — `-O2` vs `-O3` behavior
- [Google Sanitizers Wiki](https://github.com/google/sanitizers) — ASan/TSan/UBSan
- [libc++ std::regex 10x slower than libstdc++](https://github.com/llvm/llvm-project/issues/60991) — regex performance issue
- [Agner Fog: Optimizing Software in C++](https://www.agner.org/optimize/optimizing_cpp.pdf) — authoritative C++ optimization reference
- [clang-tidy performance checks](https://clang.llvm.org/extra/clang-tidy/checks/performance/inefficient-string-concatenation.html) — string concatenation pitfalls
- [mimalloc Benchmarks](https://microsoft.github.io/mimalloc/bench.html) — allocator comparison data
- [nanobench GitHub](https://github.com/martinus/nanobench) — single-header benchmarking library
- [FreeBSD pmcstat man page](https://man.freebsd.org/cgi/man.cgi?query=pmcstat&sektion=8) — FreeBSD CPU profiling

### Secondary (MEDIUM confidence)
- [Johnny's Software Lab — PGO](https://johnnysswlab.com/tune-your-programs-speed-with-profile-guided-optimizations/) — practical PGO guide
- [JetBrains — LTO, PGO, Unity Builds](https://blog.jetbrains.com/clion/2022/05/testing-3-approaches-performance-cpp_apps/) — compiler technique comparisons
- [-O3 slower than -O2 analysis](https://barish.me/blog/cpp-o3-slower/) — documented -O3 regression cases
- [Xcode Instruments for C++ CPU Profiling](https://www.jviotti.com/2024/01/29/using-xcode-instruments-for-cpp-cpu-profiling.html) — macOS profiling guide
- [Algorithms for High Performance Terminal Apps (Textual)](https://textual.textualize.io/blog/2024/12/12/algorithms-for-high-performance-terminal-apps/) — TUI rendering optimization patterns
- [How Fast is Procfs](https://avagin.github.io/how-fast-is-procfs.html) — /proc filesystem performance characteristics
- [Daniel Lemire: Which is fastest: read, fread, ifstream?](https://lemire.me/blog/2012/06/26/which-is-fastest-read-fread-ifstream-or-mmap/) — I/O method benchmarks
- [String Concatenation - CPP Optimizations Diary](https://cpp-optimizations.netlify.app/strings_concatenation/) — string append benchmarks

### Tertiary (LOW/MEDIUM confidence — corroborated by higher-confidence sources)
- [C++ Performance Pitfalls overview](https://medium.com/@threehappyer/c-performance-optimization-avoiding-common-pitfalls-and-best-practices-guide-81eee8e51467) — patterns confirmed by Agner Fog reference
- [10 C++ Mistakes That Secretly Kill Your Performance](https://medium.com/codetodeploy/10-c-mistakes-that-secretly-kill-your-performance-i-was-guilty-of-4-for-years-9b4a6c5bf425) — patterns confirmed by official docs

---
*Research completed: 2026-02-27*
*Ready for roadmap: yes*
