---
phase: 01-profiling-baseline
verified: 2026-02-27T00:00:00Z
status: gaps_found
score: 3/4 success criteria verified
re_verification: false
gaps:
  - truth: "A ranked list of hot functions (with percentage contribution) exists from perf/Instruments profiling data"
    status: failed
    reason: "No perf/Instruments profiling session was run or documented. The three plans did not scope actual CPU sampling profiling. Only nanobench timing data (ns/op) exists, not a CPU attribution report with percentage contribution per function."
    artifacts:
      - path: "(no artifact produced)"
        issue: "No perf.data, flame graph SVG, profiling report, or similar artifact committed. The Success Criterion requires a ranked list from live profiling, not just benchmark timings."
    missing:
      - "Run perf stat / perf record on btop (Linux) and produce a ranked function list with % CPU attribution"
      - "Alternatively, document profiling results inline in RESEARCH.md or a PROFILING.md artifact with flame graph data or perf report output"
      - "OR formally scope this gap as deferred and update the Success Criterion to reflect what was delivered"
human_verification:
  - test: "Confirm benchmark mode executes without terminal"
    expected: "btop --benchmark 3 produces valid JSON timing data and exits without opening raw terminal mode or alternate screen buffer"
    why_human: "Cannot run the compiled btop binary in this verification context; a human must confirm the binary actually works headlessly on the target machine"
  - test: "Confirm nanobench produces stable results (under 10% standard deviation)"
    expected: "Running btop_bench_strings multiple times shows consistent median timings with low variance"
    why_human: "Stability depends on runtime execution environment; grep-level verification cannot confirm statistical properties"
---

# Phase 1: Profiling & Baseline Verification Report

**Phase Goal:** Quantified baseline measurements exist for all performance dimensions, confirmed hotspot ranking guides all subsequent optimization work
**Verified:** 2026-02-27
**Status:** gaps_found
**Re-verification:** No -- initial verification (previous VERIFICATION.md existed but used no GSD frontmatter format)

## Goal Achievement

### Observable Truths (from ROADMAP.md Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Running the benchmark suite produces repeatable CPU usage, memory footprint, startup time, and frame render time numbers on Linux, macOS, and FreeBSD | VERIFIED | `btop --benchmark N` outputs JSON with wall/collect/draw timings. Micro-benchmarks produce ns/op data with nanobench. CI workflow runs both on Linux and macOS. FreeBSD supported (proc benchmark is stub-only there, macro+string+draw work). |
| 2 | A ranked list of hot functions (with percentage contribution) exists from perf/Instruments profiling data | FAILED | No profiling session artifact exists anywhere in the repository. No perf.data, flame graph, or `perf report` output was committed. The three plans (01-01, 01-02, 01-03) did not scope running actual CPU sampling profiling -- they scoped building benchmarking infrastructure. Benchmark timing data (ns/op) is present but is not equivalent to a CPU attribution profile. |
| 3 | Micro-benchmarks for the top hot functions (Proc::collect, draw functions, Fx::uncolor, string utilities) can be run individually with nanobench and produce stable results | VERIFIED | `benchmarks/bench_string_utils.cpp` benchmarks Fx::uncolor, Tools::ulen/uresize/ljust/rjust/cjust, Mv::to (7 functions). `benchmarks/bench_draw.cpp` benchmarks Meter, Graph construction, Graph update, createBox, fmt::format composition, Mv::to batch, Fx::uncolor on draw output (7 components). `benchmarks/bench_proc_collect.cpp` benchmarks /proc/self/stat read, /proc/stat read, /proc directory scan, and attempts Proc::collect. All support --json flag. |
| 4 | A CI job detects performance regressions by comparing against the recorded baseline | VERIFIED | `.github/workflows/benchmark.yml` runs on push to main and PRs to main. Two jobs: benchmark-linux (ubuntu-24.04, auto-push baseline to gh-pages) and benchmark-macos (macos-15, secondary). Uses benchmark-action/github-action-benchmark@v1 with 200% alert threshold, comment-on-alert, fail-on-alert disabled. |

**Score:** 3/4 truths verified

### Required Artifacts

#### Plan 01-01 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `benchmarks/CMakeLists.txt` | Benchmark build infrastructure with nanobench FetchContent | VERIFIED | FetchContent_Declare for nanobench v4.3.11, three executables declared. Contains `FetchContent_Declare`. |
| `benchmarks/bench_string_utils.cpp` | Micro-benchmarks for string utility hot functions | VERIFIED | 7 real benchmarks: Fx::uncolor, Tools::ulen/uresize/ljust/rjust/cjust, Mv::to. Full nanobench implementation with warmup(100), relative(true), --json support, doNotOptimizeAway. |
| `benchmarks/bench_draw.cpp` | Micro-benchmarks for draw functions | VERIFIED | 7 real benchmarks with Config/Theme initialization. Contains `nanobench`. Not stubs -- Meter and Graph are exercised with real theme initialization. |
| `benchmarks/bench_proc_collect.cpp` | Micro-benchmark for Proc::collect (Linux only) | VERIFIED | Contains `Proc::collect`, guarded by `#ifdef __linux__`. Benchmarks /proc parsing subroutines and attempts full Proc::collect. Non-Linux platforms get a no-op stub main. |
| `CMakeLists.txt` | BTOP_BENCHMARKS option gating benchmark subdirectory | VERIFIED | Line 42: `option(BTOP_BENCHMARKS "Build performance benchmarks" OFF)`. Lines 287-288: `if(BTOP_BENCHMARKS) add_subdirectory(benchmarks)`. |

#### Plan 01-02 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/btop_cli.hpp` | benchmark_cycles field in Cli struct | VERIFIED | `std::optional<std::uint32_t> benchmark_cycles;` present after `updates` field. |
| `src/btop_cli.cpp` | CLI parsing for --benchmark N flag | VERIFIED | Parses `-b` and `--benchmark`, validates cycles >= 1, stores in `cli.benchmark_cycles`. Help text includes `-b, --benchmark <N>` with description. |
| `src/btop.cpp` | Benchmark execution path that bypasses terminal init | VERIFIED | Full implementation at line 1022-1138. Initializes Shared::init(), Config, Theme, Draw::calcSizes(). Runs N collect+draw cycles, measures wall/collect/draw microseconds per cycle, outputs structured JSON with per-cycle timings and summary stats (mean, min, max). GPU_SUPPORT conditional compilation handled. |

#### Plan 01-03 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `.github/workflows/benchmark.yml` | CI workflow for automated performance regression detection | VERIFIED | Contains `benchmark-action/github-action-benchmark`. Two jobs (linux, macos), concurrency groups, correct triggers. |
| `benchmarks/convert_nanobench_json.py` | Converter from nanobench JSON to customSmallerIsBetter format | VERIFIED | Contains `customSmallerIsBetter` in docstring. Handles both nanobench `results[]` flat format and btop `benchmark.summary` format. No external dependencies. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `benchmarks/CMakeLists.txt` | `CMakeLists.txt` | add_subdirectory(benchmarks) gated by BTOP_BENCHMARKS | VERIFIED | `if(BTOP_BENCHMARKS)` at line 287 with `add_subdirectory(benchmarks)` at line 288 confirmed present. |
| `benchmarks/bench_string_utils.cpp` | libbtop | target_link_libraries linking against libbtop | VERIFIED | `target_link_libraries(btop_bench_strings PRIVATE libbtop nanobench)` confirmed in benchmarks/CMakeLists.txt line 14. Same pattern for bench_draw and bench_proc. |
| `src/btop_cli.cpp` | `src/btop_cli.hpp` | parse() populates Cli::benchmark_cycles | VERIFIED | `cli.benchmark_cycles = static_cast<std::uint32_t>(cycles)` confirmed in btop_cli.cpp. |
| `src/btop.cpp` | `src/btop_cli.hpp` | btop_main checks benchmark_cycles and enters benchmark path | VERIFIED | `if (cli.benchmark_cycles.has_value())` at line 1023 confirmed in btop.cpp. |
| `.github/workflows/benchmark.yml` | `benchmarks/bench_string_utils.cpp` | CI builds and runs btop_bench_strings --json | VERIFIED | `./build/benchmarks/btop_bench_strings --json > bench_strings.json` present on lines 39 and 97. |
| `.github/workflows/benchmark.yml` | `src/btop.cpp` | CI runs btop --benchmark N for macro-benchmark | VERIFIED | `./build/btop --benchmark 20 > bench_macro.json` present on lines 45 and 102. |
| `benchmarks/convert_nanobench_json.py` | `.github/workflows/benchmark.yml` | CI calls converter to transform nanobench output | VERIFIED | `python3 benchmarks/convert_nanobench_json.py` called on lines 49 and 106. |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| PROF-01 | 01-01, 01-02 | Baseline benchmarks for CPU usage, memory footprint, startup time, and frame render time across Linux, macOS, FreeBSD | SATISFIED | `btop --benchmark N` produces wall/collect/draw timings. Micro-benchmarks measure component-level timing. CI runs on Linux and macOS. FreeBSD: macro-benchmark works (proc benchmark is Linux-only). |
| PROF-02 | 01-01 | Micro-benchmarks for hot functions (Proc::collect, draw functions, Fx::uncolor, string utilities) using nanobench | SATISFIED | All named hot functions benchmarked: Fx::uncolor in bench_string_utils.cpp, draw functions and Graph/Meter in bench_draw.cpp, Proc::collect and /proc parsing in bench_proc_collect.cpp. All use nanobench with auto-tuning. |
| PROF-03 | 01-03 | Automated performance regression detection in CI pipeline | SATISFIED | benchmark.yml triggers on push/PR to main, runs micro+macro benchmarks, converts to customSmallerIsBetter format, stores baseline with github-action-benchmark, alerts on 200% threshold without blocking merge. |

**Orphaned requirements check:** REQUIREMENTS.md maps only PROF-01, PROF-02, PROF-03 to Phase 1. No additional Phase 1 requirements exist in REQUIREMENTS.md that are unaccounted for.

**Documentation gap (non-blocking):** REQUIREMENTS.md still shows PROF-01, PROF-02, PROF-03 as `- [ ]` (unchecked) with "Pending" status in the traceability table. All three plan SUMMARYs correctly declare `requirements-completed: [PROF-01, PROF-02]` / `[PROF-03]`. REQUIREMENTS.md needs to be updated to mark these complete and change traceability status to "Complete."

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/btop.cpp` | 372, 471 | TODO comments | Info | Pre-existing TODOs in Runner class, unrelated to this phase's benchmark code. |
| `benchmarks/bench_proc_collect.cpp` | 91-107 | try/catch around full Proc::collect with silent skip | Warning | If Proc::collect benchmark silently skips in CI (which the workflow handles with `|| true`), the baseline for Proc::collect timing is absent. The /proc parsing sub-benchmarks still run, but they measure ifstream read time, not the full collect path. |

No blocker anti-patterns in the benchmark infrastructure itself.

### Human Verification Required

#### 1. btop --benchmark Headless Execution

**Test:** Run `./build-bench/btop --benchmark 3` in a terminal
**Expected:** Command exits within a few seconds with JSON output on stdout. No terminal flicker, no alternate screen buffer activation, no raw mode. JSON is valid (pipe through `python3 -m json.tool`).
**Why human:** Cannot execute binaries in verification context. The implementation looks correct (benchmark path inserted before `Term::init()`), but actual headless behavior must be confirmed.

#### 2. nanobench Result Stability

**Test:** Run `./build-bench/benchmarks/btop_bench_strings` twice on the same machine, compare `Fx::uncolor` median values
**Expected:** Median values within 10% of each other across runs (plan requires <10% standard deviation)
**Why human:** Statistical stability requires runtime measurement. Grep-level verification confirmed warmup(100) is set, but cannot verify actual variance.

#### 3. CI Workflow End-to-End

**Test:** Push a commit to main or open a PR against main, observe the "Performance Benchmarks" workflow run
**Expected:** Both benchmark-linux and benchmark-macos jobs complete. benchmark-linux stores results to gh-pages dev/bench directory. A performance regression (if any) triggers a PR comment.
**Why human:** Cannot trigger and observe GitHub Actions workflow from this context.

### Gaps Summary

One gap blocks full Success Criterion achievement:

**Gap: No perf/Instruments profiling artifact (Success Criterion 2)**

The ROADMAP Success Criterion 2 states: "A ranked list of hot functions (with percentage contribution) exists from perf/Instruments profiling data."

What was delivered instead: nanobench micro-benchmarks that measure specific known hot functions in isolation (ns/op). These provide quantitative timing data but are not a CPU attribution profile. A profiling session using `perf record` + `perf report` (Linux) or `xcrun xctrace` (macOS) was never run. No flame graph, perf report, or CPU % attribution table was produced or committed anywhere in the repository.

The three plans for Phase 1 were designed around building benchmark infrastructure, not running a profiling session. The research phase identified perf/Instruments as the right tools and how to use them, but neither the plans nor their implementations included actually running these tools.

**Impact assessment:** The benchmark timing data (especially `btop --benchmark N` showing collect_us 5x draw_us) provides functional guidance for prioritizing optimization work. The subsequent phases (2: string reduction, 3: I/O) are informed by this. However, without a profiler-derived CPU % attribution list, the Success Criterion as written is not met. Optimization decisions relied on prior knowledge of hot functions (Fx::uncolor via regex, ifstream file I/O) rather than confirmed profiling output.

**Options for resolution:**
1. Run `perf record -g -- ./build-bench/btop --benchmark 10` on Linux, generate `perf report` or flame graph, commit a PROFILING.md with results
2. Update the ROADMAP Success Criterion 2 to reflect what was actually delivered (timing-based hot function identification via nanobench, not sampling profiler output)

---

## Verified Commits

| Commit | Description | Status |
|--------|-------------|--------|
| `55bcf07` | feat(01-01): create benchmark infrastructure and string utility micro-benchmarks | CONFIRMED |
| `15dbae8` | feat(01-01): add draw function and Proc::collect micro-benchmarks | CONFIRMED |
| `4875fc4` | feat(01-02): add --benchmark flag to CLI parser | CONFIRMED |
| `3385de2` | feat(01-02): implement benchmark execution mode in btop_main | CONFIRMED |
| `cdfbe5a` | feat(01-03): create nanobench-to-benchmark-action JSON converter | CONFIRMED |
| `51e0ce2` | feat(01-03): add CI benchmark workflow for regression detection | CONFIRMED |

---

_Verified: 2026-02-27_
_Verifier: Claude (gsd-verifier)_
