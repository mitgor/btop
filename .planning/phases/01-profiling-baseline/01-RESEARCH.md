# Phase 1: Profiling & Baseline - Research

**Researched:** 2026-02-27
**Domain:** C++ performance profiling, micro-benchmarking, and CI regression detection for btop++
**Confidence:** HIGH

## Summary

Phase 1 establishes the quantified baseline that every subsequent optimization phase depends on. The work divides into four distinct deliverables: (1) a reproducible macro-benchmark suite that measures CPU usage, memory footprint, startup time, and frame render time on Linux, macOS, and FreeBSD; (2) profiling with platform-native tools (perf/Instruments/pmcstat) to produce a ranked hotspot list; (3) nanobench-based micro-benchmarks for the confirmed hot functions (Proc::collect, draw functions, Fx::uncolor, string utilities like uresize/ljust/rjust); and (4) a CI job using github-action-benchmark that detects regressions against the recorded baseline.

The project already has a test infrastructure in place: GoogleTest v1.17.0 via FetchContent, a `tests/` directory with one test file (`tools.cpp`), and CI workflows on Linux (cmake-linux.yml with GCC 14, Clang 19-21), macOS (cmake-macos.yml with Homebrew LLVM), and FreeBSD (cmake-freebsd.yml via vmactions). The CMakeLists.txt includes `include(CTest)` with `BUILD_TESTING` gating the test subdirectory. This existing infrastructure can be extended for benchmarks by adding a parallel `benchmarks/` directory following the same FetchContent + CTest pattern.

The primary challenge is that btop is an interactive terminal application -- it cannot be benchmarked end-to-end without either (a) a `--benchmark` mode that runs N cycles headlessly and exits, or (b) a wrapper that launches btop, waits, and kills it. The recommended approach is to add a `--benchmark <N>` CLI flag that runs N collect+draw cycles to `/dev/null` without terminal initialization, then prints timing JSON to stdout. This gives stable, repeatable measurements without external dependencies. For micro-benchmarks, the hot functions are already accessible through the `libbtop` OBJECT library target, which can be linked directly into benchmark executables just as it is already linked into test executables.

**Primary recommendation:** Add a `benchmarks/` directory with nanobench micro-benchmarks linked against `libbtop`, add a `--benchmark` flag for end-to-end measurement, and integrate github-action-benchmark with `customSmallerIsBetter` JSON format into the existing CI matrix.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PROF-01 | Baseline benchmarks established for CPU usage, memory footprint, startup time, and frame render time across Linux, macOS, and FreeBSD | Macro-benchmark suite via `--benchmark` mode + hyperfine for startup; CI workflows already exist for all three platforms; RelWithDebInfo + `-fno-omit-frame-pointer` build preset enables profiling |
| PROF-02 | Micro-benchmarks created for identified hot functions (Proc::collect, draw functions, Fx::uncolor, string utilities) using nanobench | nanobench single-header integration via FetchContent; `libbtop` OBJECT library allows direct linking; hot function signatures identified with stable interfaces for isolated benchmarking |
| PROF-03 | Automated performance regression detection integrated into CI pipeline | github-action-benchmark with `customSmallerIsBetter` format; nanobench JSON output; threshold-based alerting on PR commits; baseline stored in gh-pages branch or external JSON |
</phase_requirements>

## Standard Stack

### Core

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| **nanobench** | 4.3+ (latest v4.3.11) | Single-header micro-benchmarking for hot functions | ~80x faster than Google Benchmark, single header (zero new build dependencies), C++11-23 compatible, produces JSON output for CI integration. Fits btop's "no heavy dependencies" philosophy. Already recommended in STACK.md research. **Confidence: HIGH** |
| **hyperfine** | 1.18+ | End-to-end startup time and full-cycle benchmarking from command line | Statistical rigor (warmup, outlier detection, multiple runs), comparison mode, JSON output. Cross-platform (Linux, macOS, FreeBSD via cargo or package managers). **Confidence: HIGH** |
| **perf** | 6.x (matches kernel) | Linux CPU sampling profiler for hotspot identification | Zero-copy kernel integration, <2% overhead, gives per-function CPU attribution. The gold standard for Linux C++ profiling. **Confidence: HIGH** |
| **Instruments (xctrace)** | Xcode 16+ | macOS CPU Time Profiler and Allocations | Only hardware-level profiler on macOS ARM64. CLI accessible via `xcrun xctrace record`. **Confidence: HIGH** |
| **pmcstat** | FreeBSD base system | FreeBSD hardware performance counter profiling | FreeBSD's equivalent of Linux perf. Uses hwpmc(4) kernel module. **Confidence: MEDIUM** (less community tooling) |
| **github-action-benchmark** | v1 | CI performance regression detection | Supports custom JSON format (`customSmallerIsBetter`), stores baselines in gh-pages or external JSON, configurable alert thresholds. Active maintenance, 2.7k+ stars. **Confidence: HIGH** |

### Supporting

| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| **heaptrack** | 1.5+ | Heap allocation profiling with call stacks | Linux only. Use to measure allocation count per cycle and identify allocation hotspots. Run on the `--benchmark` mode to get per-cycle allocation data. |
| **Hotspot** (KDAB) | 1.5.0+ | GUI visualization for perf.data | Use instead of raw `perf report` for flame graphs and source annotation. |
| **Brendan Gregg's FlameGraph** | latest | Generate flame graph SVGs from profiler output | Universal tool -- works with perf, pmcstat, DTrace. Use for all platforms. |
| **strace / dtruss** | System tools | System call counting | Use `strace -c` on Linux to count syscalls per cycle. `dtruss` on macOS (requires SIP disabled). |
| **Valgrind Callgrind** | 3.23+ | Instruction-level profiling | Use only when sampling (perf) doesn't explain WHY a function is hot. 10-50x overhead makes it unsuitable for real-time measurement. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| nanobench | Google Benchmark 1.9+ | More features (fixtures, parametric) but requires linking a library, slower compilation. nanobench is sufficient for A/B testing individual functions. |
| github-action-benchmark | Bencher.dev | Full-featured SaaS for continuous benchmarking. Overkill for this project; github-action-benchmark is free, lightweight, and sufficient. |
| Custom `--benchmark` flag | External test harness (e.g., expect/tmux) | External approach is fragile, platform-dependent, and can't measure individual collect/draw phases. Internal mode is more reliable and gives finer-grained data. |
| hyperfine | Custom timing script | hyperfine handles warmup, statistical analysis, and output formatting. No reason to hand-roll. |

**Installation:**
```bash
# nanobench: No install needed -- FetchContent in CMake (single header downloaded at build time)

# Linux
sudo apt install linux-tools-common linux-tools-$(uname -r)  # perf
sudo apt install heaptrack heaptrack-gui                      # allocation profiling
sudo apt install hyperfine                                    # startup benchmarking
# Hotspot: AppImage from https://github.com/KDAB/hotspot/releases
# FlameGraph: git clone https://github.com/brendangregg/FlameGraph.git

# macOS
brew install hyperfine
# Instruments comes with Xcode (xcrun xctrace)

# FreeBSD
sudo kldload hwpmc  # enable hardware perf counters
sudo pkg install hyperfine
# pmcstat is in base system
```

## Architecture Patterns

### Recommended Project Structure

```
benchmarks/
+-- CMakeLists.txt              # FetchContent nanobench, benchmark executables
+-- bench_string_utils.cpp      # Micro-benchmarks for uresize, ljust, rjust, ulen, uncolor
+-- bench_proc_collect.cpp      # Micro-benchmark for Proc::collect (Linux-only)
+-- bench_draw.cpp              # Micro-benchmarks for draw functions (Graph::operator(), Meter)
+-- bench_main.cpp              # Shared nanobench main with JSON output support
src/
+-- btop_cli.cpp                # Add --benchmark <N> flag parsing
+-- btop.cpp                    # Add benchmark mode: N cycles to /dev/null, print timing JSON
.github/
+-- workflows/
    +-- benchmark.yml           # New: runs benchmarks, compares to baseline, alerts on regression
```

### Pattern 1: Benchmark Executable Linked Against libbtop

**What:** Create benchmark executables that link against the existing `libbtop` OBJECT library, just as the test executable does. This gives benchmark code direct access to all btop functions without duplicating source files.

**When to use:** For all micro-benchmarks (string utils, draw functions).

**Example:**
```cmake
# benchmarks/CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG v4.3.11
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nanobench)

add_executable(btop_bench_strings bench_string_utils.cpp)
target_link_libraries(btop_bench_strings PRIVATE libbtop nanobench)
target_include_directories(btop_bench_strings PRIVATE ${PROJECT_SOURCE_DIR}/src)
```

**Source: [nanobench installation docs](https://nanobench.ankerl.com/tutorial.html)**

### Pattern 2: nanobench JSON Output for CI Integration

**What:** Configure nanobench to suppress console output and render results as JSON, which github-action-benchmark can consume via `customSmallerIsBetter` tool type.

**When to use:** When running benchmarks in CI for regression detection.

**Example:**
```cpp
// bench_string_utils.cpp
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>
#include "btop_tools.hpp"

int main() {
    // Prepare test data
    std::string test_str(1000, 'x');
    // Insert some ANSI escape codes
    for (size_t i = 0; i < test_str.size(); i += 20) {
        test_str.insert(i, "\033[38;5;196m");
    }

    ankerl::nanobench::Bench bench;
    bench.title("String Utilities");
    bench.output(nullptr); // suppress console for CI

    bench.run("Fx::uncolor", [&] {
        auto result = Fx::uncolor(test_str);
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    bench.run("Tools::ulen", [&] {
        auto result = Tools::ulen(test_str);
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    // Render as JSON for CI consumption
    bench.render(ankerl::nanobench::templates::json(), std::cout);
    return 0;
}
```

**Source: [nanobench tutorial](https://nanobench.ankerl.com/tutorial.html)**

### Pattern 3: --benchmark Mode for End-to-End Measurement

**What:** Add a `--benchmark <N>` CLI flag that runs N collect+draw cycles without terminal initialization, outputting timing data as JSON to stdout. This provides a stable, repeatable macro-benchmark.

**When to use:** For CI regression detection and manual baseline measurement.

**Why:** btop is interactive -- it initializes a terminal, handles signals, and runs an event loop. A benchmark mode bypasses terminal init, runs the Runner's collect+draw sequence N times, and prints structured timing data (per-cycle wall time, per-box collect time, per-box draw time, total output bytes).

**Example approach:**
```cpp
// In btop.cpp, after CLI parsing:
if (cli.benchmark_cycles > 0) {
    // Initialize config and shared state (needed by collectors)
    // Do NOT init terminal
    Shared::init();

    // Run N cycles, timing each phase
    for (int i = 0; i < cli.benchmark_cycles; i++) {
        auto t0 = Tools::time_micros();
        auto& cpu = Cpu::collect();
        auto t1 = Tools::time_micros();
        auto cpu_out = Cpu::draw(cpu, /*gpus,*/ true, false);
        auto t2 = Tools::time_micros();
        // ... repeat for Mem, Net, Proc
        // Record timings
    }
    // Print JSON summary to stdout
    return 0;
}
```

### Pattern 4: CMake Build Preset for Profiling

**What:** Add a CMake preset (or documented flags) for profiling builds: `RelWithDebInfo` with `-fno-omit-frame-pointer` to enable perf stack walking while keeping optimizations active.

**When to use:** Always when profiling. Never profile Debug builds (unrepresentative) or Release builds without debug info (no symbol resolution).

**Example:**
```bash
# Profiling build
cmake -B build-profile \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer" \
    -DBUILD_TESTING=ON
cmake --build build-profile
```

### Anti-Patterns to Avoid

- **Profiling Debug builds:** Debug builds have optimizations disabled (`-O0`), which makes them 5-20x slower than release and creates completely different bottleneck profiles. Always profile `RelWithDebInfo`.
- **Using gprof:** Obsolete. Requires recompilation with `-pg`, doesn't handle shared libraries, misleading with modern optimizations, doesn't support multithreaded code. Use `perf` instead.
- **Measuring wall-clock time with `time`:** `time` doesn't account for warmup, variance, or system load. Use `hyperfine` for startup timing and nanobench for function timing.
- **Benchmarking on a loaded system:** Other processes skew results. Use `taskset` to pin to specific cores and minimize background load, or use nanobench's automatic outlier detection.
- **Profiling with LTO disabled:** btop enables LTO in release builds. Profiling without LTO gives a different inlining landscape. Use `RelWithDebInfo` which enables LTO by default in btop's CMakeLists.txt.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Micro-benchmark timing | Manual `chrono::high_resolution_clock` loops | nanobench | Handles warmup, outlier detection, statistical analysis, prevents dead code elimination. Manual timing is consistently wrong. |
| Startup time measurement | `time ./btop` in a script | hyperfine | Statistical rigor, warmup runs, comparison mode, JSON output. `time` gives a single noisy sample. |
| CI regression detection | Custom script comparing JSON files | github-action-benchmark | Handles baseline storage, threshold comparison, alert comments, trend visualization. Maintained by community. |
| Flame graph generation | Manual perf script + awk | FlameGraph scripts (stackcollapse-perf.pl + flamegraph.pl) | Canonical implementation, handles edge cases in stack trace formats, interactive SVG output. |
| JSON output from benchmarks | Manual `cout << "{\"name\": ..."` | nanobench `templates::json()` renderer | Produces well-formed JSON with all statistics. Manual JSON is error-prone and incomplete. |

**Key insight:** The profiling phase is entirely about measurement infrastructure. Every "build vs. buy" decision should favor existing tools because measurement accuracy and reproducibility matter more than in application code.

## Common Pitfalls

### Pitfall 1: Non-Reproducible Benchmark Results

**What goes wrong:** Benchmark numbers vary wildly between runs (>10% standard deviation), making regression detection useless.
**Why it happens:** System load variation, CPU frequency scaling, thermal throttling, background processes, filesystem caching effects.
**How to avoid:** (1) Use `hyperfine --warmup 3 --min-runs 10` for startup benchmarks. (2) For micro-benchmarks, nanobench auto-tunes iteration count for stable results. (3) In CI, use dedicated runners or accept higher variance thresholds (200% default in github-action-benchmark). (4) For manual profiling, disable CPU frequency scaling (`cpupower frequency-set -g performance` on Linux) and pin to cores with `taskset`.
**Warning signs:** Standard deviation >5% on micro-benchmarks; startup time varying by >15% between runs.

### Pitfall 2: Benchmarking the Wrong Build Configuration

**What goes wrong:** Profiling a Debug build produces misleading hotspot rankings because compiler optimizations change which code is actually hot.
**Why it happens:** Debug builds use `-O0`, which disables inlining, loop unrolling, and dead code elimination. Functions that are inlined away in Release appear as hotspots in Debug.
**How to avoid:** Always profile `RelWithDebInfo` builds. Verify with `cmake --build build --verbose | grep -E '\-O[0-9]'` that optimization flags are active.
**Warning signs:** Profiling shows standard library internals (allocator, string constructors) as top hotspots instead of btop functions.

### Pitfall 3: Micro-Benchmarks That Don't Reflect Real Usage

**What goes wrong:** Micro-benchmark of Fx::uncolor shows it's fast, but real-world profiling shows it's slow.
**Why it happens:** Micro-benchmarks test with small inputs or hot caches. Real usage processes 10-100KB strings with cold caches.
**How to avoid:** Use realistic input sizes derived from actual btop output. Measure the output string size of a real btop frame (capture with `--benchmark` mode) and use strings of that size in micro-benchmarks.
**Warning signs:** Micro-benchmark improvement doesn't translate to end-to-end improvement.

### Pitfall 4: Platform-Specific Profiling Gaps

**What goes wrong:** Baseline only exists for Linux. macOS regression goes undetected for months.
**Why it happens:** perf doesn't exist on macOS. Instruments requires Xcode and has different output format. FreeBSD pmcstat is less familiar.
**How to avoid:** The macro-benchmark (--benchmark mode + hyperfine) works on all platforms. Micro-benchmarks (nanobench) are cross-platform. Only the profiler-based hotspot analysis (PROF-02) is platform-specific. CI benchmark job must run on all three platform matrices.
**Warning signs:** Benchmark CI job only triggers on Linux runners.

### Pitfall 5: Incomplete Hotspot List Due to Missing Frame Pointers

**What goes wrong:** perf shows `[unknown]` in stack traces instead of function names.
**Why it happens:** Compiler omits frame pointers with optimization enabled (default for both GCC and Clang with -O2). Without frame pointers, perf can't walk the stack reliably.
**How to avoid:** Build with `-fno-omit-frame-pointer` for profiling. This is already standard practice but must be explicitly set in the CMake profiling configuration. Also ensure `-g` is present (RelWithDebInfo provides this).
**Warning signs:** More than 5% of samples in perf report show `[unknown]` frames.

### Pitfall 6: CI Benchmark Job That Blocks Development

**What goes wrong:** Benchmark CI job is too sensitive (low threshold) or too slow, causing false-positive failures that block PRs.
**Why it happens:** CI environments have noisy performance characteristics (shared runners, variable CPU allocation). Default threshold may be too tight.
**How to avoid:** Start with a generous threshold (200%, the github-action-benchmark default) and tighten gradually as baseline stabilizes. Mark benchmark job as `continue-on-error: true` initially, alerting via comment rather than blocking. Use `alert-comment-cc-users` to notify maintainers without blocking merge.
**Warning signs:** Benchmark job fails on unrelated PRs; developers start ignoring benchmark alerts.

## Code Examples

### Example 1: nanobench Integration for String Utilities

```cpp
// benchmarks/bench_string_utils.cpp
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>
#include "btop_tools.hpp"

int main(int argc, char* argv[]) {
    bool json_output = (argc > 1 && std::string(argv[1]) == "--json");

    // Prepare realistic test data (typical btop output line with ANSI codes)
    std::string colored_line;
    for (int i = 0; i < 80; i++) {
        colored_line += "\033[38;5;" + std::to_string(i % 256) + "m" + "X";
    }
    colored_line += "\033[0m";

    std::string plain_line(200, 'A'); // 200 UTF-8 chars

    ankerl::nanobench::Bench bench;
    bench.title("String Utilities")
         .warmup(100)
         .relative(true);

    if (json_output) bench.output(nullptr);

    bench.run("Fx::uncolor (colored line)", [&] {
        auto result = Fx::uncolor(colored_line);
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    bench.run("Tools::ulen (plain)", [&] {
        auto result = Tools::ulen(plain_line);
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    bench.run("Tools::uresize (to 50)", [&] {
        auto result = Tools::uresize(plain_line, 50);
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    bench.run("Tools::ljust (pad to 100)", [&] {
        auto result = Tools::ljust("short string", 100);
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    bench.run("Tools::rjust (pad to 100)", [&] {
        auto result = Tools::rjust("short string", 100);
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    bench.run("Mv::to(10, 50)", [&] {
        auto result = Mv::to(10, 50);
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    if (json_output) {
        bench.render(ankerl::nanobench::templates::json(), std::cout);
    }

    return 0;
}
```

### Example 2: benchmarks/CMakeLists.txt

```cmake
# benchmarks/CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG v4.3.11
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nanobench)

# String utility benchmarks (cross-platform)
add_executable(btop_bench_strings bench_string_utils.cpp)
target_link_libraries(btop_bench_strings PRIVATE libbtop nanobench)
target_include_directories(btop_bench_strings PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Draw function benchmarks (cross-platform)
add_executable(btop_bench_draw bench_draw.cpp)
target_link_libraries(btop_bench_draw PRIVATE libbtop nanobench)
target_include_directories(btop_bench_draw PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Process collection benchmarks (platform-specific, Linux only for now)
if(LINUX)
    add_executable(btop_bench_proc bench_proc_collect.cpp)
    target_link_libraries(btop_bench_proc PRIVATE libbtop nanobench)
    target_include_directories(btop_bench_proc PRIVATE ${PROJECT_SOURCE_DIR}/src)
endif()
```

### Example 3: CI Benchmark Workflow

```yaml
# .github/workflows/benchmark.yml
name: Performance Benchmarks
on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  benchmark-linux:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6

      - name: Configure
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer" \
            -DBUILD_TESTING=ON \
            -DBTOP_BENCHMARKS=ON

      - name: Build
        run: cmake --build build

      - name: Run Benchmarks
        run: |
          build/benchmarks/btop_bench_strings --json > bench_results.json

      - name: Store Benchmark Result
        uses: benchmark-action/github-action-benchmark@v1
        with:
          tool: 'customSmallerIsBetter'
          output-file-path: bench_results.json
          github-token: ${{ secrets.GITHUB_TOKEN }}
          auto-push: ${{ github.ref == 'refs/heads/main' }}
          alert-threshold: '150%'
          comment-on-alert: true
          fail-on-alert: false
          alert-comment-cc-users: '@aristocratos'
```

### Example 4: perf Profiling Session (Manual, Linux)

```bash
# Build for profiling
cmake -B build-profile \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer"
cmake --build build-profile

# Record 30 seconds of btop execution
# (requires --benchmark mode or manual interaction)
perf record -g --call-graph dwarf -F 997 -- ./build-profile/btop --benchmark 100

# Generate flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > btop_flamegraph.svg

# Or view in Hotspot GUI
hotspot perf.data
```

### Example 5: hyperfine Startup Benchmark

```bash
# Measure startup time with statistical analysis
# btop needs --benchmark 1 (single cycle then exit) for this
hyperfine \
    --warmup 3 \
    --min-runs 20 \
    --export-json startup_bench.json \
    './build-profile/btop --benchmark 1'
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| gprof for C++ profiling | perf + flame graphs | ~2015 | gprof is obsolete; perf provides hardware-level sampling without recompilation |
| Manual timing with chrono | nanobench / Google Benchmark | ~2018 | Automatic warmup, outlier detection, dead code elimination prevention |
| No CI regression detection | github-action-benchmark / Bencher | ~2020 | Automated regression detection on every PR |
| `-O3` for maximum performance | `-O2 + PGO` | Ongoing (documented 2024-2025) | `-O3` can cause regressions via code bloat; PGO gives better results |
| Valgrind as primary profiler | perf for sampling, Valgrind for targeted analysis | ~2016 | Valgrind's 10-50x overhead distorts real-time behavior |

**Deprecated/outdated:**
- **gprof**: Obsolete. Does not support multithreading, misleading with modern optimizations.
- **Valgrind on macOS ARM64**: Deprecated. Use Instruments instead.

## Open Questions

1. **How to handle Proc::collect micro-benchmarking given /proc dependency**
   - What we know: Proc::collect reads from `/proc` filesystem, which is Linux-specific and requires real processes to exist. It cannot be benchmarked with synthetic data easily.
   - What's unclear: Whether isolating just the parsing logic (given a pre-read buffer) is sufficient, or if the full I/O path must be benchmarked in-situ.
   - Recommendation: Two-level approach. (1) Micro-benchmark the parsing logic by reading `/proc/[pid]/stat` content into a string and benchmarking the parse function in isolation. (2) Macro-benchmark full Proc::collect via `--benchmark` mode which exercises the real I/O path. This gives both the parsing cost (nanobench) and the total I/O cost (macro-benchmark).

2. **nanobench JSON compatibility with github-action-benchmark**
   - What we know: nanobench produces its own JSON format. github-action-benchmark expects `customSmallerIsBetter` format: `[{"name": "...", "unit": "...", "value": N}]`.
   - What's unclear: Whether nanobench's JSON output matches this format directly.
   - Recommendation: Likely needs a small conversion script or a custom nanobench output template. The conversion is trivial (extract `name` and `median` from nanobench JSON, emit in the expected format). Write a small Python or jq script for CI.

3. **Initial state dependencies for draw function benchmarking**
   - What we know: Draw functions depend on initialized Config, Theme, and box dimensions. They read `Config::getB/getS/getI` frequently. Graph and Meter objects need initialization.
   - What's unclear: Whether `Shared::init()` + `Config::load()` is sufficient setup for isolated draw benchmarking, or if the full btop init sequence is required.
   - Recommendation: Investigate during implementation. Start with minimal init (Config::load + Term dimensions set manually) and add dependencies as needed. The existing DebugTimer usage in the codebase (commented-out in GPU collection) suggests the developers have explored this pattern before.

4. **FreeBSD CI benchmark feasibility**
   - What we know: FreeBSD CI uses `vmactions/freebsd-vm@v1` which runs FreeBSD in a VM. The existing cmake-freebsd.yml compiles and runs CTest.
   - What's unclear: Whether VM overhead makes benchmark results too noisy for regression detection.
   - Recommendation: Start with Linux and macOS benchmarks in CI. Add FreeBSD benchmarks experimentally and measure variance. If variance is >20%, use FreeBSD benchmarks for compilation-only verification and rely on manual profiling for FreeBSD performance data.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 (via FetchContent) + nanobench v4.3.11 (via FetchContent) |
| Config file | `tests/CMakeLists.txt` (existing), `benchmarks/CMakeLists.txt` (new, Wave 0) |
| Quick run command | `ctest --test-dir build -R bench_strings --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure && build/benchmarks/btop_bench_strings --json > results.json` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PROF-01 | Baseline benchmarks produce repeatable CPU, memory, startup, and render time numbers | smoke + benchmark | `build/benchmarks/btop_bench_strings --json && hyperfine --min-runs 5 './build/btop --benchmark 10'` | No -- Wave 0 |
| PROF-01 | Benchmark results are repeatable (low variance) | benchmark validation | `build/benchmarks/btop_bench_strings --json` (check stddev <5% in output) | No -- Wave 0 |
| PROF-02 | Micro-benchmarks for Fx::uncolor, uresize, ljust, rjust, ulen, Mv::to run individually | unit benchmark | `build/benchmarks/btop_bench_strings` | No -- Wave 0 |
| PROF-02 | Micro-benchmarks for draw functions (Graph::operator(), Meter) run individually | unit benchmark | `build/benchmarks/btop_bench_draw` | No -- Wave 0 |
| PROF-02 | Micro-benchmarks for Proc::collect run individually (Linux) | unit benchmark | `build/benchmarks/btop_bench_proc` | No -- Wave 0 |
| PROF-02 | Ranked hotspot list exists from perf/Instruments profiling | manual | Manual: `perf record` + `perf report` / `xcrun xctrace record` | N/A -- manual-only |
| PROF-03 | CI job runs benchmarks and compares against baseline | integration | `gh workflow run benchmark.yml` (verify pass) | No -- Wave 0 |
| PROF-03 | CI job alerts on performance regression exceeding threshold | integration | Trigger by submitting PR with intentionally slow code | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build --output-on-failure` (existing tests pass) + `build/benchmarks/btop_bench_strings` (benchmarks compile and run)
- **Per wave merge:** Full benchmark suite: all bench executables with `--json` output, verify results are within expected ranges
- **Phase gate:** All benchmark executables produce stable results (stddev <10%); CI benchmark workflow passes; hotspot list documented

### Wave 0 Gaps
- [ ] `benchmarks/CMakeLists.txt` -- benchmark build infrastructure with nanobench FetchContent
- [ ] `benchmarks/bench_string_utils.cpp` -- covers PROF-02 (string utility micro-benchmarks)
- [ ] `benchmarks/bench_draw.cpp` -- covers PROF-02 (draw function micro-benchmarks)
- [ ] `benchmarks/bench_proc_collect.cpp` -- covers PROF-02 (Proc::collect micro-benchmark, Linux only)
- [ ] `CMakeLists.txt` update -- add `BTOP_BENCHMARKS` option gating `add_subdirectory(benchmarks)`
- [ ] `src/btop_cli.cpp` / `src/btop.cpp` -- add `--benchmark <N>` mode for PROF-01 macro-benchmarks
- [ ] `.github/workflows/benchmark.yml` -- covers PROF-03 (CI regression detection)
- [ ] Conversion script for nanobench JSON -> github-action-benchmark format (if needed)

## Sources

### Primary (HIGH confidence)
- [nanobench GitHub](https://github.com/martinus/nanobench) -- single-header benchmarking library, API and CMake integration
- [nanobench installation docs](https://nanobench.ankerl.com/tutorial.html) -- FetchContent integration, JSON output, benchmark API
- [github-action-benchmark](https://github.com/benchmark-action/github-action-benchmark) -- CI regression detection, custom format support, baseline storage
- [Brendan Gregg's perf Examples](https://www.brendangregg.com/perf.html) -- Linux perf profiling reference
- [KDAB Hotspot GitHub](https://github.com/KDAB/hotspot) -- perf.data visualization
- [KDE heaptrack GitHub](https://github.com/KDE/heaptrack) -- heap allocation profiling
- [hyperfine GitHub](https://github.com/sharkdp/hyperfine) -- command-line benchmarking tool
- [Brendan Gregg's FlameGraph](https://github.com/brendangregg/FlameGraph) -- flame graph generation
- btop++ source code -- direct analysis of `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/tools.cpp`, `.github/workflows/*.yml`, `src/btop_tools.hpp`, `src/btop_draw.hpp`, `src/btop_shared.hpp`, `src/linux/btop_collect.cpp`

### Secondary (MEDIUM confidence)
- [Continuous Benchmark GitHub Marketplace](https://github.com/marketplace/actions/continuous-benchmark) -- alternative docs for github-action-benchmark
- [FreeBSD pmcstat man page](https://man.freebsd.org/cgi/man.cgi?query=pmcstat&sektion=8) -- FreeBSD profiling
- [Xcode Instruments for C++ CPU Profiling](https://www.jviotti.com/2024/01/29/using-xcode-instruments-for-cpp-cpu-profiling.html) -- macOS profiling guide

### Tertiary (LOW confidence)
- nanobench JSON -> customSmallerIsBetter format compatibility: not verified with actual tool output; flagged as needing validation during implementation

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all tools verified via official docs and GitHub repos; versions confirmed current
- Architecture: HIGH -- based on direct analysis of btop source, CMakeLists.txt, existing test infrastructure, and CI workflows
- Pitfalls: HIGH -- pitfalls 1-4 derived from profiling best practices with official documentation support; pitfalls 5-6 from project-specific CI analysis

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable domain -- profiling tools don't change frequently)
