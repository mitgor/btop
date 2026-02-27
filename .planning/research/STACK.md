# Stack Research

**Domain:** C++ terminal application performance optimization (btop++)
**Researched:** 2026-02-27
**Confidence:** HIGH

## Recommended Stack

This stack is organized around the optimization workflow: profile, analyze, optimize, verify. Every tool earns its place by solving a specific problem in that chain. btop++ is a C++23 CMake project targeting Linux, macOS, and FreeBSD, built with GCC 12+ or Clang 15+, using fmt for formatting and pthreads for concurrency.

### Core Profiling Tools

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **Linux `perf`** | 6.x (matches kernel) | CPU sampling profiler, hardware counters, cache analysis | Zero-copy kernel integration, negligible overhead (<2%), gives wall-clock AND CPU-clock profiles. The gold standard for Linux C++ profiling. Every other Linux profiler either wraps perf or is worse. **Confidence: HIGH** |
| **Apple Instruments** (xctrace) | Xcode 16+ | CPU Time Profiler, Allocations, System Trace on macOS | Only real option on macOS for hardware-level profiling. Time Profiler gives per-function CPU attribution; Allocations tracks heap churn. Command-line via `xcrun xctrace record`. **Confidence: HIGH** |
| **FreeBSD `pmcstat`** | Base system | Hardware performance counter profiling on FreeBSD | FreeBSD's equivalent of Linux perf. Uses hwpmc(4) kernel module. Run `kldload hwpmc` first. Produces stack traces compatible with flame graph generation. **Confidence: MEDIUM** (less community tooling than perf) |
| **Valgrind Callgrind** | 3.23+ | Instruction-level profiling, call graph analysis | Simulates CPU execution — counts exact instruction/cache miss numbers, not statistical samples. Slow (10-50x overhead) but gives precise call counts. Use for "which function is called how many times" questions that sampling misses. **Confidence: HIGH** |
| **Valgrind Cachegrind** | 3.23+ | Cache miss profiling (L1/L2/LL) | Simulates cache hierarchy to count misses per source line. Critical for data layout optimization — L1 miss costs ~10 cycles, L2 miss costs ~200 cycles. Use after perf identifies hot functions to understand WHY they're hot. **Confidence: HIGH** |

### Visualization Tools

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **Hotspot** (KDAB) | 1.5.0+ | GUI for Linux perf data with flame graphs | Best perf.data visualizer available. Built-in flame graphs, source annotation, timeline view. AppImage available for any Linux distro. Click-to-source integration. Use instead of raw `perf report`. **Confidence: HIGH** |
| **Brendan Gregg's FlameGraph** | latest (Perl scripts) | Generate flame graph SVGs from any profiler output | Universal flame graph generator — works with perf, DTrace, pmcstat. Interactive SVGs let you zoom into hot paths. The canonical tool. **Confidence: HIGH** |
| **KCachegrind** | 23.x+ | GUI for Callgrind/Cachegrind output | Visualizes call graphs and per-line cache miss data from Valgrind tools. Essential companion to Callgrind — raw text output is nearly unusable for complex programs. **Confidence: HIGH** |
| **speedscope** | 0.7+ | Web-based flame graph viewer | Alternative to Hotspot when you need portability or are on macOS. Imports perf, Chrome trace, and other formats. Lightweight — just open in browser. **Confidence: MEDIUM** |

### Memory Analysis Tools

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **heaptrack** (KDE) | 1.5+ | Heap allocation profiler | Tracks every malloc/free with full call stacks. Much faster than Valgrind Massif — doesn't serialize threads, only adds overhead on allocation (not CPU computation). Shows allocation hotspots, temporary allocations, and peak memory contributors. Use this first for memory work. **Confidence: HIGH** |
| **Valgrind Massif** | 3.23+ | Heap memory profiling over time | Produces time-series snapshots of heap usage. Slower than heaptrack but produces cleaner "memory over time" graphs. Use when heaptrack's timeline isn't sufficient. **Confidence: HIGH** |
| **heaptrack_gui** | 1.5+ | GUI for heaptrack data | Visualizes heaptrack output with flame graphs of allocations, timeline of heap size, and per-callstack breakdown. Essential — heaptrack text output alone is insufficient. **Confidence: HIGH** |
| **AddressSanitizer (ASan)** | GCC 12+ / Clang 15+ | Memory error detection | 2-3x overhead. Detects buffer overflows, use-after-free, leaks. Compile with `-fsanitize=address`. Not a profiler per se, but catching memory bugs before optimizing prevents wasted effort. **Confidence: HIGH** |

### Micro-Benchmarking

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **nanobench** | 4.3+ | Single-header micro-benchmarking | ~80x faster iteration than Google Benchmark, single-header (no dependency), C++11/17/20 compatible. Use for A/B testing specific optimizations (e.g., "is fmt::format_to faster than fmt::format here?"). Fits btop's "no heavy dependencies" constraint. **Confidence: HIGH** |
| **Google Benchmark** | 1.9+ | Full-featured micro-benchmarking | More features than nanobench (fixtures, parametric, threading). But requires linking a library, slower compilation. Use only if nanobench's feature set is insufficient. **Confidence: MEDIUM** — overkill for this project |

### Compiler Optimization Techniques

| Technique | Flags / Config | Purpose | Why Recommended |
|-----------|---------------|---------|-----------------|
| **Link-Time Optimization (LTO)** | CMake: already enabled via `BTOP_LTO` | Whole-program optimization across compilation units | btop already has this in CMakeLists.txt. LTO enables cross-TU inlining and dead code elimination. Verify it's actually active in release builds. **Confidence: HIGH** |
| **Profile-Guided Optimization (PGO)** | GCC: `-fprofile-generate` / `-fprofile-use`; Clang: `-fprofile-instr-generate` / `-fprofile-instr-use` | Use runtime profiles to guide compiler optimization | PGO is the single highest-impact compiler technique for real-world performance. Compiler uses actual execution data to decide inlining, branch prediction, code layout. Typical gains: 10-20% for CPU-bound workloads. Requires a representative profiling run. **Confidence: HIGH** |
| **Optimization Level** | `-O2` (preferred) or `-O3` (test carefully) | Compiler optimization level | `-O2` is the safe default. `-O3` adds aggressive vectorization and inlining that can HURT performance (code bloat, instruction cache pressure). For btop, `-O2 + LTO + PGO` will almost certainly outperform `-O3` alone. Always benchmark both. **Confidence: HIGH** |
| **Architecture-specific** | `-march=native` | Target current CPU's instruction set | Enables AVX/SSE/NEON where profitable. Only for local development/benchmarking — release builds must be portable. Never ship with `-march=native`. **Confidence: HIGH** |

### Runtime Analysis Tools

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **ThreadSanitizer (TSan)** | GCC 12+ / Clang 15+ | Data race detection | btop uses pthreads. TSan detects races with 5-15x overhead. Compile with `-fsanitize=thread`. Fix races before optimizing concurrency — a race condition that "works" today will break after optimization changes timing. **Confidence: HIGH** |
| **UndefinedBehaviorSanitizer (UBSan)** | GCC 12+ / Clang 15+ | Detect undefined behavior | ~1.2x overhead. Catches signed overflow, null deref, misalignment. Compile with `-fsanitize=undefined`. UB that "works" in debug builds can produce wrong results after optimization. **Confidence: HIGH** |
| **strace / dtruss** | System tools | System call tracing | strace (Linux), dtruss (macOS) show every syscall. If btop is doing excessive read/write/ioctl calls, these reveal it instantly. Low effort, high diagnostic value for I/O bottlenecks. **Confidence: HIGH** |

### Supporting Libraries (Optimization-Adjacent)

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **mimalloc** | 2.1+ | Drop-in memory allocator replacement | If heaptrack reveals high allocation churn, swap glibc malloc for mimalloc via `LD_PRELOAD`. Outperforms jemalloc and tcmalloc across allocation sizes in 2025 benchmarks. Header-only mode available. Test with `LD_PRELOAD=libmimalloc.so btop` — zero code changes needed to evaluate. **Confidence: HIGH** |
| **Tracy Profiler** | 0.11+ | Real-time frame profiler with nanosecond resolution | If btop needs continuous profiling during development (like a game engine), Tracy adds ~2ns per instrumented zone. Requires source instrumentation (macros). Overkill unless the frame-by-frame timing of btop's render loop needs constant monitoring. **Confidence: MEDIUM** — high quality but requires code changes |
| **{fmt}** (already used) | 11.x (header-only) | String formatting | btop already uses fmt. Key optimization: switch from `fmt::format` (returns `std::string`) to `fmt::format_to` (writes to existing buffer) in hot paths to eliminate allocations. This is a code-level optimization, not a library swap. **Confidence: HIGH** |

## Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| **CMake** (already used) | Build system | Add CMake presets for PGO workflow: `pgo-generate`, `pgo-use`. Add a `RelWithDebInfo` preset for profiling (optimized + debug symbols). |
| **compile_commands.json** | IDE integration, static analysis | Already enabled via `CMAKE_EXPORT_COMPILE_COMMANDS`. Ensure clangd or clang-tidy can consume it. |
| **clang-tidy** | Static analysis for performance | Run `clang-tidy` with `performance-*` checks to catch unnecessary copies, inefficient container usage, etc. Automated low-hanging fruit finder. |
| **Compiler Explorer (godbolt.org)** | Inspect generated assembly | When optimizing hot loops, paste the function into Compiler Explorer to verify the compiler is vectorizing / inlining as expected. Free, browser-based, no setup. |
| **hyperfine** | Command-line benchmarking | Measure btop startup time with statistical rigor: `hyperfine --warmup 3 './btop --tty_on'`. Handles variance, warmup, comparison. |

## Installation

```bash
# Linux (Ubuntu/Debian) — core profiling stack
sudo apt install linux-tools-common linux-tools-$(uname -r)  # perf
sudo apt install valgrind kcachegrind                         # Valgrind + GUI
sudo apt install heaptrack heaptrack-gui                      # heap profiling
sudo apt install strace                                       # syscall tracing
sudo apt install hyperfine                                    # startup benchmarking

# Hotspot — use AppImage for latest version
wget https://github.com/KDAB/hotspot/releases/latest/download/hotspot-x86_64.AppImage
chmod +x hotspot-x86_64.AppImage

# FlameGraph scripts
git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph

# mimalloc (for testing)
sudo apt install libmimalloc-dev

# macOS — core profiling stack
# Instruments comes with Xcode (xcrun xctrace)
brew install hyperfine

# FreeBSD — core profiling stack
sudo kldload hwpmc            # enable hardware perf counters
# pmcstat is in base system
sudo pkg install valgrind kcachegrind hyperfine

# Compiler flags for profiling builds (add to CMake)
# cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer" ..

# PGO workflow (GCC example)
# Step 1: Instrument build
# cmake -DCMAKE_CXX_FLAGS="-fprofile-generate=/tmp/btop-pgo" ..
# Step 2: Run btop for representative workload
# Step 3: Optimized build
# cmake -DCMAKE_CXX_FLAGS="-fprofile-use=/tmp/btop-pgo" ..
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| **perf** (Linux) | Intel VTune | Only if targeting Intel-specific optimization (e.g., AVX tuning). VTune is free but proprietary, heavy install, Intel-only insights. perf is sufficient for btop's needs. |
| **heaptrack** | Valgrind Massif | When you need time-series "heap over time" visualization rather than allocation-site analysis. Massif's ms_print output is cleaner for temporal patterns. |
| **mimalloc** | jemalloc / tcmalloc | jemalloc appears increasingly unmaintained as of 2025. tcmalloc is viable but requires Abseil/Google infrastructure. mimalloc is simpler, faster for mixed workloads, and works as `LD_PRELOAD` without code changes. |
| **nanobench** | Google Benchmark | Only if you need parametric benchmarks, fixtures, or threading support that nanobench lacks. For A/B testing individual optimizations, nanobench is faster to set up and compile. |
| **Hotspot** | Firefox Profiler | Firefox Profiler can import perf data and runs in browser (good for remote servers). But Hotspot's source annotation and timeline are superior for C++ work. |
| **`-O2` + PGO** | `-O3` alone | Almost never. `-O3` without PGO frequently underperforms `-O2` with PGO due to code bloat increasing instruction cache pressure. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **gprof** | Obsolete. Requires recompilation with `-pg`, doesn't handle shared libraries, misleading results with modern optimizations, doesn't support multi-threaded code properly. | Linux perf (sampling, no recompilation needed) |
| **`-O3` blindly** | `-O3` enables aggressive loop vectorization and inlining that can increase code size, cause instruction cache thrashing, and actually slow down programs. Documented cases of `-O3` being slower than `-O2` exist in 2025 compiler versions. | `-O2` as baseline, benchmark `-O3` per-TU if needed, always prefer PGO over raising -O level |
| **`-march=native` in release builds** | Produces binaries that crash on CPUs lacking the instruction set used at build time. btop is distributed software — users compile on one machine and may run on another. | Use `-march=native` only for local benchmarking, not in shipped CMake defaults |
| **jemalloc** | Increasingly unmaintained as of 2025. tcmalloc has Google backing, mimalloc has Microsoft backing. jemalloc's advantage in multi-threaded arena scaling doesn't justify the maintenance risk for a new integration. | mimalloc (simpler, faster for mixed workloads, actively maintained) |
| **Valgrind as the primary profiler** | 10-50x slowdown makes it unsuitable for profiling btop's real-time rendering loop. Results are distorted because the timing characteristics of a 50x-slower program differ fundamentally from real execution. | Linux perf for sampling (real-time, low overhead), Valgrind only for instruction-counting and cache simulation |
| **Tracy without deliberate need** | Requires instrumenting source code with macros. High-quality tool, but the overhead of maintaining instrumentation in a project that doesn't need continuous real-time profiling isn't justified. btop isn't a game engine. | perf + Hotspot for periodic profiling sessions |
| **Custom allocators (writing one)** | Engineering effort is enormous and almost never beats battle-tested allocators. mimalloc/tcmalloc represent decades of optimization. | LD_PRELOAD mimalloc to test; only pursue custom allocation pools for specific proven hotspots |

## Stack Patterns by Optimization Phase

**Phase 1 — Baseline & Profiling:**
- Build with `RelWithDebInfo` + `-fno-omit-frame-pointer` (enables perf stack walking)
- Profile with perf (Linux), Instruments (macOS), pmcstat (FreeBSD)
- Visualize with Hotspot / FlameGraph scripts
- Measure startup with hyperfine
- Run heaptrack for allocation baseline
- Because: you cannot optimize what you haven't measured

**Phase 2 — String & Memory Optimization:**
- Use heaptrack to find allocation hotspots (expect `btop_draw.cpp` and `fmt::format` calls)
- Use Cachegrind to identify cache-unfriendly data access patterns
- Benchmark individual changes with nanobench
- Test mimalloc via `LD_PRELOAD` as a quick win
- Because: btop's ~207 fmt::format calls and heavy string construction in the 2517-line draw module are likely allocation-heavy

**Phase 3 — Rendering & I/O Optimization:**
- Use strace/dtruss to count write() syscalls per frame
- Use perf to profile the render path specifically
- Because: terminal output is buffered but each write() syscall still has kernel overhead; reducing syscall count can significantly reduce CPU time

**Phase 4 — Compiler-Level Optimization:**
- Implement PGO workflow in CMake
- Benchmark `-O2+PGO` vs `-O3+PGO` vs `-O2` vs `-O3`
- Because: PGO is the highest-ROI compiler technique and should be applied after code-level optimizations are done (so the profile reflects the optimized code)

**Phase 5 — Verification:**
- Run full sanitizer suite (ASan, TSan, UBSan) on optimized build
- Re-profile to confirm improvements
- Compare against Phase 1 baseline
- Because: optimizations that introduce UB or races are not optimizations

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| perf 6.x | Linux kernel 6.x | Version must match running kernel |
| Valgrind 3.23+ | GCC 12-14, Clang 15-19 | Valgrind must support the instruction set used; check release notes for new CPU support |
| heaptrack 1.5+ | GCC 12+, Clang 15+ | Requires debug symbols (`-g`) for useful output |
| Hotspot 1.5.0 | perf 5.x+ data files | Uses perf_event_open; needs `perf_event_paranoid <= 1` on Linux |
| mimalloc 2.1+ | GCC 12+, Clang 15+, all platforms | Works on Linux, macOS, FreeBSD. LD_PRELOAD on Linux; DYLD_INSERT_LIBRARIES on macOS |
| nanobench 4.3+ | C++11 minimum, C++20/23 compatible | Single header, no build system integration needed |
| {fmt} 11.x | C++17 minimum, C++23 compatible | Already in btop as header-only (FMT_HEADER_ONLY defined) |

## Platform-Specific Notes

### Linux
- Full tooling support. perf + Hotspot + heaptrack + Valgrind all work natively.
- Set `kernel.perf_event_paranoid=1` (or `=0`) in `/etc/sysctl.conf` for non-root perf access.
- Build with `-fno-omit-frame-pointer` for reliable perf stack traces (Clang/GCC default to omitting frame pointers with optimization).

### macOS
- No perf, no Valgrind (deprecated on ARM64), no heaptrack.
- Primary tool: Instruments via `xcrun xctrace record --template 'Time Profiler' --launch ./btop`.
- For memory: Instruments Allocations template, or `leaks` / `heap` command-line tools.
- mimalloc testing: `DYLD_INSERT_LIBRARIES=/path/to/libmimalloc.dylib ./btop`.
- DTrace available but requires SIP disabled for full functionality.

### FreeBSD
- pmcstat for CPU profiling, DTrace for tracing.
- Valgrind works but has historically lagged behind Linux support.
- Load hwpmc kernel module before profiling: `kldload hwpmc`.
- FlameGraph scripts work with pmcstat output.

## Sources

- [KDAB C and C++ Profiling Tools Overview](https://www.kdab.com/c-cpp-profiling-tools/) — Comprehensive profiling tool survey (verified)
- [Brendan Gregg's perf Examples](https://www.brendangregg.com/perf.html) — Authoritative Linux perf reference (HIGH confidence)
- [Brendan Gregg's FlameGraph](https://github.com/brendangregg/FlameGraph) — Flame graph generation tooling (HIGH confidence)
- [KDAB Hotspot GitHub](https://github.com/KDAB/hotspot) — Latest release info, v1.5.0 (HIGH confidence)
- [KDE heaptrack GitHub](https://github.com/KDE/heaptrack) — Heap profiler documentation (HIGH confidence)
- [Valgrind Massif Manual](https://valgrind.org/docs/manual/ms-manual.html) — Official documentation (HIGH confidence)
- [mimalloc Benchmarks](https://microsoft.github.io/mimalloc/bench.html) — Microsoft's allocator benchmark data (HIGH confidence)
- [Johnny's Software Lab — PGO](https://johnnysswlab.com/tune-your-programs-speed-with-profile-guided-optimizations/) — Practical PGO guide (MEDIUM confidence)
- [LLVM PGO Documentation](https://llvm.org/docs/HowToBuildWithPGO.html) — Official Clang PGO docs (HIGH confidence)
- [GCC Optimization Options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html) — Official GCC docs (HIGH confidence)
- [Google Sanitizers Wiki](https://github.com/google/sanitizers) — ASan/TSan/UBSan documentation (HIGH confidence)
- [Brendan Gregg's FreeBSD Flame Graphs](https://www.brendangregg.com/blog/2015-03-10/freebsd-flame-graphs.html) — FreeBSD profiling workflow (MEDIUM confidence — 2015 but techniques still apply)
- [FreeBSD pmcstat man page](https://man.freebsd.org/cgi/man.cgi?query=pmcstat&sektion=8) — Official documentation (HIGH confidence)
- [nanobench GitHub](https://github.com/martinus/nanobench) — Single-header benchmarking library (HIGH confidence)
- [Xcode Instruments for C++ CPU Profiling](https://www.jviotti.com/2024/01/29/using-xcode-instruments-for-cpp-cpu-profiling.html) — Practical macOS profiling guide (MEDIUM confidence)
- [JetBrains — LTO, PGO, Unity Builds](https://blog.jetbrains.com/clion/2022/05/testing-3-approaches-performance-cpp_apps/) — Comparative benchmarks of compiler techniques (MEDIUM confidence)
- [-O3 slower than -O2 analysis](https://barish.me/blog/cpp-o3-slower/) — Documented cases of -O3 regression (MEDIUM confidence)
- [Clang ThreadSanitizer Documentation](https://clang.llvm.org/docs/ThreadSanitizer.html) — Official docs (HIGH confidence)

---
*Stack research for: C++ terminal application performance optimization (btop++)*
*Researched: 2026-02-27*
