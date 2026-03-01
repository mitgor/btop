# Phase 19: Performance Measurement - Research

**Researched:** 2026-03-01
**Domain:** C++ application benchmarking, CPU/memory profiling, reproducible measurement methodology
**Confidence:** HIGH

## Summary

Phase 19 is a measurement-only phase -- no code changes, only building binaries, running benchmarks, and documenting results. The project already has extensive benchmark infrastructure from v1.0 (Phase 1): a built-in `--benchmark N` CLI mode that runs N collect+draw cycles and outputs per-cycle JSON timing data, nanobench microbenchmarks, and a Python comparison script. Existing benchmark results from v1.0 show a ~10% collect time improvement and +14% RSS increase (deliberate: pre-allocated buffers).

The key gap is that the existing comparison only measured v1.0 changes (v1.4.6 vs end-of-v1.0). Phase 19 must measure the **cumulative** impact of v1.0+v1.1+v1.2, comparing upstream v1.4.6 against the current HEAD (post-v1.2). The v1.1 FSM refactor added ~13K lines of code and replaced global atomics with variant-based state machines -- this could affect both CPU and memory. The v1.2 tech debt cleanup was architectural (routing, dead code removal) with minimal expected performance impact.

**Primary recommendation:** Reuse the existing `benchmark_comparison.py` methodology (backport `--benchmark` flag to v1.4.6, run both binaries, compare JSON output), update it to use current HEAD as the "new" binary, add external `/usr/bin/time` measurement for validation, and produce a committed PERFORMANCE.md report.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PERF-01 | CPU usage measured before/after v1.0+v1.1 optimizations with documented methodology | Existing `--benchmark` mode provides per-cycle collect/draw timing. External `/usr/bin/time` provides user+sys CPU time. Methodology from `benchmark_comparison.py` is proven. |
| PERF-02 | Memory footprint measured before/after v1.0+v1.1 optimizations with documented methodology | `os.wait4()` provides peak RSS via `ru_maxrss`. External `/usr/bin/time -l` provides peak RSS validation. Both approaches documented in existing scripts. |
</phase_requirements>

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| btop `--benchmark N` | Built-in (Phase 1) | Per-cycle collect/draw timing in JSON | Measures actual hot path without terminal I/O overhead |
| `/usr/bin/time -l` | System | User/sys CPU time + peak RSS | OS-level measurement, no instrumentation overhead |
| `os.wait4()` (Python) | System | Peak RSS via `ru_maxrss` | Direct rusage access from fork+exec, more reliable than parsing /usr/bin/time |
| Python 3 `statistics` | stdlib | Mean, median, stdev computation | No external dependency needed |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `script(1)` | System | Provide pseudo-tty for btop | Only needed for non-benchmark mode measurement (btop requires a tty for normal operation) |
| CMake + Ninja | 4.2.3 / 1.13.2 | Build both baseline and current binaries | Required for reproducible builds with identical flags |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `--benchmark` mode | External process measurement only | `--benchmark` gives per-cycle granularity; external gives overall CPU time. Use BOTH. |
| Python script | bash script | Python handles JSON parsing, statistics, and report generation more cleanly |
| `powermetrics` | Nothing | macOS-only, requires sudo, adds complexity -- skip for reproducibility |

## Architecture Patterns

### Measurement Script Structure
```
scripts/
  measure_performance.py     # Main comparison driver
docs/
  PERFORMANCE.md             # Committed results + methodology
```

### Pattern 1: Backport Benchmark Flag to Baseline
**What:** Cherry-pick only the `--benchmark` CLI mode (~99 lines in 3 files) onto the v1.4.6 tag, build, and use for internal timing comparison.
**When to use:** Always -- this is how the v1.0 comparison was done and it works.
**Existing proof:** `/tmp/btop-old/` contains v1.4.6 + backported `--benchmark` (git diff shows +99 lines in btop.cpp, btop_cli.cpp, btop_cli.hpp).

### Pattern 2: Dual Measurement Approach
**What:** Measure using BOTH `--benchmark` mode (internal per-cycle timing) AND external `/usr/bin/time` (OS-level CPU+memory). Cross-validate results.
**When to use:** Always -- internal and external measurements complement each other.
**Why:** `--benchmark` mode gives microsecond-precision per-cycle timing but doesn't capture startup/teardown. External measurement captures total CPU time including startup but lacks cycle-level detail.

### Pattern 3: Warmup + Multiple Runs
**What:** Discard first N cycles (cold-start effects), run multiple complete benchmark invocations, aggregate all warm cycles.
**Configuration (from existing scripts):**
- 50 cycles per run, 5 warmup cycles discarded = 45 measured per run
- 6 runs = 270 total measured cycles
- Report mean, median, stdev, min, max

### Anti-Patterns to Avoid
- **Comparing debug builds:** Always use RelWithDebInfo or Release with identical flags for both binaries
- **Measuring on battery:** Ensure consistent power state (plugged in, no thermal throttling)
- **Single-run measurements:** Insufficient for statistical significance -- use 6+ runs
- **Mixing measurement methods:** Don't compare `--benchmark` internal timing from one version with `/usr/bin/time` from another
- **Running other workloads during measurement:** Close applications, disable Spotlight, minimize background activity

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Per-cycle timing | Custom instrumentation | Built-in `--benchmark` mode | Already exists, measures actual collect+draw paths |
| Peak RSS measurement | /proc parsing or manual sampling | `os.wait4()` -> `ru_maxrss` | Single system call, accurate for peak lifetime RSS |
| Statistical analysis | Manual averaging | `statistics.mean/median/stdev` | Correct implementations, handles edge cases |
| Build comparison | Manual cmake invocations | Script with exact cmake flags | Reproducibility requires identical build configuration |

## Common Pitfalls

### Pitfall 1: RSS Measurement on macOS Returns Bytes, Not KB
**What goes wrong:** `ru_maxrss` returns bytes on macOS but kilobytes on Linux. Dividing by 1024 on macOS gives KB, not MB.
**Why it happens:** Platform API difference.
**How to avoid:** Divide by `1024 * 1024` on macOS for MB. The existing scripts do this correctly.
**Warning signs:** RSS values that look 1000x too large or too small.

### Pitfall 2: v1.4.6 Doesn't Have --benchmark
**What goes wrong:** Trying to run `btop --benchmark 50` on a vanilla v1.4.6 build fails.
**Why it happens:** The `--benchmark` flag was added in Phase 1 of v1.0.
**How to avoid:** Backport the benchmark CLI code (~99 lines) to v1.4.6. This is the established approach.
**Warning signs:** "Unknown flag" or exit code 1 from baseline binary.

### Pitfall 3: Old Baseline Binary May Not Exist
**What goes wrong:** The `/tmp/btop-old/build-bench/btop` binary from the v1.0 comparison may have been cleaned up.
**Why it happens:** /tmp is ephemeral.
**How to avoid:** The measurement script must build both binaries from source as part of the measurement process. Document exact git checkout, cmake flags, and build commands.
**Warning signs:** Script assumes pre-built binaries exist at hardcoded paths.

### Pitfall 4: Inconsistent Build Flags
**What goes wrong:** Baseline built with `-O2`, current with `-O3` (or vice versa), making comparison meaningless.
**Why it happens:** Different CMakeLists.txt versions may have different defaults.
**How to avoid:** Specify explicit, identical cmake flags for both builds: `-DCMAKE_BUILD_TYPE=Release -DBTOP_LTO=ON`.

### Pitfall 5: v1.1 Added ~13K LOC
**What goes wrong:** Expecting memory to stay the same or decrease after v1.1.
**Why it happens:** v1.1 added FSM infrastructure, event queue, 279 tests worth of type system. Binary size and baseline RSS may increase.
**How to avoid:** Document this as expected behavior. The optimization target was correctness/maintainability, not memory reduction. CPU measurements are the primary v1.1 metric (FSM dispatch should be zero-cost or near-zero-cost vs flag-based dispatch).

## Code Examples

### Build Commands (Reproducible)
```bash
# Baseline (v1.4.6 with backported --benchmark)
git worktree add /tmp/btop-baseline v1.4.6
cd /tmp/btop-baseline
# Apply benchmark backport patch (3 files, ~99 lines)
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBTOP_LTO=ON \
  -DBTOP_GPU=ON
cmake --build build

# Current (HEAD)
cd /Users/mit/Documents/GitHub/btop
cmake -B build-perf -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBTOP_LTO=ON \
  -DBTOP_GPU=ON
cmake --build build-perf
```

### Internal Benchmark Measurement
```python
# Source: benchmark_comparison.py (existing)
result = subprocess.run(
    [binary, "--benchmark", str(cycles)],
    capture_output=True, text=True, timeout=120
)
bench_data = json.loads(result.stdout)
timings = bench_data["benchmark"]["timings"]
# Discard warmup
warm = timings[WARMUP_CYCLES:]
walls = [t["wall_us"] for t in warm]
```

### Peak RSS via os.wait4()
```python
# Source: benchmark_comparison.py (existing)
def measure_peak_rss(binary, cycles):
    pid = os.fork()
    if pid == 0:
        devnull = os.open(os.devnull, os.O_WRONLY)
        os.dup2(devnull, 1)
        os.dup2(devnull, 2)
        os.close(devnull)
        os.execv(binary, [binary, "--benchmark", str(cycles)])
        os._exit(1)
    _, _, rusage = os.wait4(pid, 0)
    return rusage.ru_maxrss  # bytes on macOS
```

### JSON Output Format (from --benchmark)
```json
{
  "benchmark": {
    "cycles": 50,
    "platform": "macos",
    "timings": [
      {"cycle": 1, "wall_us": 1625.3, "collect_us": 1502.1, "draw_us": 123.2},
      ...
    ],
    "summary": {
      "mean_wall_us": 1600.0,
      "mean_collect_us": 1480.0,
      "mean_draw_us": 120.0,
      "min_wall_us": 1431.2,
      "max_wall_us": 1816.3
    }
  }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| External-only measurement | --benchmark + external dual approach | v1.0 Phase 1 | Per-cycle granularity |
| Hardcoded binary paths | Script builds from source | Needed for Phase 19 | Reproducibility |
| v1.0-only comparison | v1.0+v1.1+v1.2 cumulative comparison | Phase 19 | Complete picture |

### Existing Results (v1.0 only, for reference)
From `optimised.md` and `benchmark_results/comparison_report.json`:

| Metric | v1.4.6 | Optimized (v1.0) | Change |
|--------|--------|-------------------|--------|
| Wall time (mean) | 1,687 us | 1,512 us | -10.4% |
| Collect time (mean) | 1,560 us | 1,385 us | -11.2% |
| Draw time (mean) | 127 us | 127 us | -0.2% |
| Peak RSS (mean) | 11.7 MB | 13.3 MB | +14.0% |
| Wall time stdev | 169 us | 69 us | -59% (much more stable) |

**Important:** These numbers were v1.0-only. Phase 19 must measure HEAD (post-v1.2) vs v1.4.6.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GTest 1.17.0 (for existing tests); Python script for benchmarks |
| Config file | `tests/CMakeLists.txt` (tests); `benchmarks/CMakeLists.txt` (microbenchmarks) |
| Quick run command | `cd build && ctest --test-dir . -R btop_test --output-on-failure` |
| Full suite command | `cd build && ctest --test-dir . --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PERF-01 | CPU measured with documented methodology | manual-only | Measurement script run + results committed | N/A - manual run |
| PERF-02 | Memory measured with documented methodology | manual-only | Same measurement script | N/A - manual run |

**Justification for manual-only:** Performance measurement is inherently environment-dependent. The deliverable is the measurement script (reproducible methodology) plus committed results. Automated CI validation would be meaningless since absolute numbers vary across machines. The CI benchmark workflow (`benchmark.yml`) already handles regression detection for ongoing changes.

### Sampling Rate
- **Per task commit:** Verify measurement script runs without errors
- **Per wave merge:** N/A (single plan phase)
- **Phase gate:** PERFORMANCE.md committed with before/after numbers

### Wave 0 Gaps
None -- existing test infrastructure covers all phase requirements. The `--benchmark` mode already exists in HEAD. The measurement is a script execution + documentation task.

## Open Questions

1. **Does /tmp/btop-old still contain a usable baseline binary?**
   - What we know: As of this research, `/tmp/btop-old/build-bench/btop` exists (v1.4.6+benchmark backport, 1.5MB arm64 binary)
   - What's unclear: Whether it will persist until execution
   - Recommendation: The measurement script should rebuild from source regardless, for reproducibility. Use git worktree or clone.

2. **Will v1.1 FSM overhead be measurable?**
   - What we know: v1.1 added ~13K LOC of FSM infrastructure. `std::variant` and `std::visit` are typically zero-cost (compiler devirtualizes). Event queue is lock-free SPSC.
   - What's unclear: Whether the additional code path complexity shows up in per-cycle timing
   - Recommendation: Measure and report honestly. If neutral or slightly negative, document the architectural benefits gained.

3. **Should we use the existing comparison_report.json numbers or re-measure?**
   - What we know: Existing data is v1.0-only, from an earlier measurement session
   - What's unclear: Whether environment has changed since those measurements
   - Recommendation: Re-measure everything from scratch with a single consistent script run. This ensures identical conditions for both binaries.

## Sources

### Primary (HIGH confidence)
- `/Users/mit/Documents/GitHub/btop/benchmark_comparison.py` -- Existing measurement methodology
- `/Users/mit/Documents/GitHub/btop/benchmark_comparison.sh` -- Existing shell-based measurement
- `/Users/mit/Documents/GitHub/btop/benchmark_results/comparison_report.json` -- Prior v1.0 results
- `/Users/mit/Documents/GitHub/btop/src/btop.cpp` lines 1253-1370 -- `--benchmark` mode implementation
- `/Users/mit/Documents/GitHub/btop/optimised.md` -- v1.0 results documentation

### Secondary (MEDIUM confidence)
- `/tmp/btop-old/` -- v1.4.6 worktree with benchmark backport (may not persist)
- `/Users/mit/Documents/GitHub/btop/.github/workflows/benchmark.yml` -- CI benchmark configuration

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all tools are system-level or already built into the project
- Architecture: HIGH -- existing measurement scripts provide proven patterns
- Pitfalls: HIGH -- based on actual experience from v1.0 measurement phase

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable -- measurement tools don't change)
