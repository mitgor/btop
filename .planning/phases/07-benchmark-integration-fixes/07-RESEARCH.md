# Phase 7: Benchmark Integration Fixes - Research

**Researched:** 2026-02-27
**Domain:** C++ benchmark code (nanobench), GitHub Actions CI workflow, POSIX I/O
**Confidence:** HIGH

## Summary

Phase 7 is a focused gap-closure phase addressing three specific integration issues identified in the v1.0 milestone audit. All three gaps are well-understood, localized, and low-risk:

1. **Stale sub-benchmarks** in `benchmarks/bench_proc_collect.cpp` (lines 44-71) use `std::ifstream` to read `/proc/self/stat` and `/proc/stat`, measuring the old I/O pattern that Phase 3 replaced with `Tools::read_proc_file()`. These sub-benchmarks must be updated to call the POSIX-based optimized path.

2. **Silent CI failure** in `.github/workflows/benchmark.yml` where the proc benchmark runs with `|| true` (line 41), meaning if the benchmark crashes or produces no output, CI passes silently with zero proc benchmark data. The convert script gracefully skips bad JSON files but never signals the *absence* of proc data.

3. **Silent try/catch swallow** in the `Proc::collect (full)` benchmark (lines 89-107 of bench_proc_collect.cpp) catches `Shared::init()` failure and prints to stderr, but this message is invisible in CI since stderr is not captured or checked.

**Primary recommendation:** This is a single-plan phase. Update bench_proc_collect.cpp sub-benchmarks to use `Tools::read_proc_file()`, replace `|| true` with explicit failure handling in CI, and add structured skip/failure reporting for the full `Proc::collect` benchmark.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PROF-02 | Micro-benchmarks for hot functions using nanobench | Sub-benchmarks must measure the actual optimized code path (read_proc_file), not the replaced ifstream pattern |
| PROF-03 | CI performance regression detection | CI workflow must not silently swallow proc benchmark failures; zero-data runs must be detectable |
| IODC-01 | Linux Proc::collect ifstream replaced with POSIX read() | Benchmark integration ensures the optimized path is actually measured and regressions caught |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| nanobench | v4.3.11 | Micro-benchmarking | Already in project via FetchContent; all 4 benchmark files use it |
| Tools::read_proc_file() | N/A (project code) | POSIX read() for /proc files | Phase 3 optimized I/O path, defined in btop_tools.hpp:386 |
| GitHub Actions | v6 (checkout) | CI automation | Existing benchmark.yml workflow |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| benchmark-action/github-action-benchmark | v1 | Benchmark result tracking | Already used in benchmark.yml for historical comparison |

### Alternatives Considered
None -- this phase modifies existing code, no new libraries needed.

## Architecture Patterns

### Existing Benchmark File Structure
```
benchmarks/
  bench_proc_collect.cpp   # TARGET: Linux-only proc benchmarks (this phase)
  bench_string_utils.cpp   # Reference: string utility benchmarks
  bench_draw.cpp           # Reference: draw component benchmarks
  bench_data_structures.cpp # Reference: data structure comparison benchmarks
  CMakeLists.txt           # Build config (no changes needed)
  convert_nanobench_json.py # JSON converter (may need enhancement)
```

### Pattern 1: read_proc_file() Stack Buffer Pattern
**What:** Read /proc pseudo-files using POSIX open/read with caller-provided stack buffer
**When to use:** Replacing ifstream reads in benchmark sub-benchmarks
**Example:**
```cpp
// Source: src/btop_tools.hpp:386-398
// Current implementation of the optimized I/O path
inline ssize_t read_proc_file(const char* path, char* buf, size_t buf_size) {
    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t total = 0;
    while (static_cast<size_t>(total) < buf_size - 1) {
        ssize_t n = ::read(fd, buf + total, buf_size - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    ::close(fd);
    buf[total] = '\0';
    return total;
}
```

### Pattern 2: Benchmark Sub-benchmark Using read_proc_file()
**What:** Replace ifstream-based sub-benchmarks with read_proc_file() calls
**When to use:** The /proc/self/stat and /proc/stat sub-benchmarks
**Example:**
```cpp
// Replace old pattern (lines 44-58):
//   std::ifstream f("/proc/self/stat");
//   std::getline(f, content);
//
// With optimized pattern:
bench.run("/proc/self/stat read+parse", [&] {
    char buf[4096];
    ssize_t n = Tools::read_proc_file("/proc/self/stat", buf, sizeof(buf));
    ankerl::nanobench::doNotOptimizeAway(n);
});
```

### Pattern 3: CI Failure Detection for Benchmarks
**What:** Replace `|| true` with structured error handling
**When to use:** CI workflow for Linux proc benchmark execution
**Example:**
```yaml
# Instead of: ./build/benchmarks/btop_bench_proc --json > bench_proc.json || true
# Option A: Remove || true, let CI fail on crash
- name: Run micro-benchmarks
  run: |
    ./build/benchmarks/btop_bench_strings --json > bench_strings.json
    ./build/benchmarks/btop_bench_draw --json > bench_draw.json
    ./build/benchmarks/btop_bench_proc --json > bench_proc.json

# Option B: Capture exit code and report explicitly
- name: Run proc micro-benchmark
  run: |
    if ! ./build/benchmarks/btop_bench_proc --json > bench_proc.json; then
      echo "::warning::Proc benchmark failed - check if /proc is accessible"
      echo '{"results":[]}' > bench_proc.json
    fi
```

### Anti-Patterns to Avoid
- **Silent || true on benchmark commands:** Masks real failures; zero data produced without any CI signal
- **Generic try/catch without structured output:** The current `catch` prints to stderr which is invisible in CI JSON collection
- **Benchmarking replaced code paths:** Measuring ifstream when the actual code uses read_proc_file() makes the benchmark meaningless for regression detection

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| JSON output formatting | Custom JSON serializer | nanobench::templates::json() | Already used by all benchmark files |
| Benchmark result storage | Custom file/DB | benchmark-action/github-action-benchmark@v1 | Already integrated in CI workflow |
| /proc file reading | New helper functions | Tools::read_proc_file() | Already implemented and tested in Phase 3 |

**Key insight:** This phase is about wiring existing pieces together correctly, not creating new functionality. Every component already exists.

## Common Pitfalls

### Pitfall 1: read_proc_file() is Linux-only (#ifdef __linux__)
**What goes wrong:** Trying to use read_proc_file() outside the `#ifdef __linux__` guard
**Why it happens:** The function is only defined for Linux in btop_tools.hpp:383-399
**How to avoid:** bench_proc_collect.cpp already has a top-level `#ifdef __linux__` guard (line 7). All changes stay inside this guard. No cross-platform concern.
**Warning signs:** Compilation errors on macOS/FreeBSD

### Pitfall 2: Empty JSON from proc benchmark breaks convert script
**What goes wrong:** If bench_proc.json contains no valid JSON (e.g., empty file from a crash), convert_nanobench_json.py hits a JSONDecodeError
**Why it happens:** The `|| true` in CI means the file may be empty or contain only error output
**How to avoid:** The convert script already handles this -- `except (json.JSONDecodeError, FileNotFoundError)` on line 67 prints a warning and continues. However, the *absence* of proc data is never flagged. The fix should either: (a) remove `|| true` so CI fails visibly, or (b) add a check step that validates bench_proc.json has results.
**Warning signs:** CI passes green but benchmark_results.json has zero proc entries

### Pitfall 3: Shared::init() requires /proc filesystem and terminal state
**What goes wrong:** Proc::collect full benchmark fails because Shared::init() checks for /proc access AND initializes Cpu/Mem collectors which read system state
**Why it happens:** CI containers (ubuntu-24.04) do have /proc access, so Shared::init() should succeed. But any environment setup issue causes silent skip.
**How to avoid:** Replace the silent catch with a structured output that produces valid-but-flagged JSON, or use a non-zero exit code with a clear message.
**Warning signs:** "Note: Full Proc::collect benchmark skipped" in stderr but no trace in CI logs

### Pitfall 4: Buffer size for /proc/stat
**What goes wrong:** /proc/stat contains one line per CPU core plus several summary lines. On machines with many cores, it can exceed a small buffer.
**Why it happens:** A 4096-byte buffer is fine for /proc/self/stat (~200 bytes) but /proc/stat grows with core count
**How to avoid:** Use a larger buffer (8192 or 16384) for /proc/stat, or read in a loop. On a 128-core machine, /proc/stat is ~6-7KB.
**Warning signs:** Truncated benchmark data, unexpectedly short content

## Code Examples

### Current Sub-benchmark (STALE -- to be replaced)
```cpp
// Source: benchmarks/bench_proc_collect.cpp:44-58
// This measures the OLD ifstream pattern, not the Phase 3 optimized path
bench.run("/proc/self/stat read+parse", [&] {
    std::ifstream f("/proc/self/stat");
    std::string content;
    std::getline(f, content);
    ankerl::nanobench::doNotOptimizeAway(content);
});
```

### Replacement Sub-benchmark (using read_proc_file)
```cpp
// Measures the ACTUAL Phase 3 optimized I/O path
bench.run("/proc/self/stat read+parse", [&] {
    char buf[4096];
    ssize_t n = Tools::read_proc_file("/proc/self/stat", buf, sizeof(buf));
    ankerl::nanobench::doNotOptimizeAway(n);
});
```

### Current /proc/stat Sub-benchmark (STALE -- to be replaced)
```cpp
// Source: benchmarks/bench_proc_collect.cpp:62-71
// Uses ifstream + getline + vector<string> -- not the optimized path
bench.run("/proc/stat read", [&] {
    std::ifstream f("/proc/stat");
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(f, line)) {
        lines.push_back(line);
    }
    ankerl::nanobench::doNotOptimizeAway(lines);
});
```

### Replacement /proc/stat Sub-benchmark
```cpp
// Measures raw POSIX read into stack buffer (the Phase 3 pattern)
bench.run("/proc/stat read", [&] {
    char buf[16384];  // /proc/stat can be large on many-core systems
    ssize_t n = Tools::read_proc_file("/proc/stat", buf, sizeof(buf));
    ankerl::nanobench::doNotOptimizeAway(n);
});
```

### Improved Proc::collect Full Benchmark Error Handling
```cpp
// Instead of silent try/catch, produce structured output
{
    bool init_success = false;
    try {
        std::vector<std::string> load_warnings;
        Config::load("", load_warnings);
        Shared::init();
        init_success = true;
    } catch (const std::exception& e) {
        std::cerr << "WARNING: Shared::init() failed: " << e.what() << std::endl;
        std::cerr << "Proc::collect full benchmark will be SKIPPED." << std::endl;
        // If JSON output requested, still produce valid structure with skip note
    }

    if (init_success) {
        bench.minEpochIterations(3);
        bench.run("Proc::collect (full)", [&] {
            auto& result = Proc::collect(false);
            ankerl::nanobench::doNotOptimizeAway(result);
        });
    } else if (!json_output) {
        std::cout << "SKIP: Proc::collect (full) -- Shared::init() failed" << std::endl;
    }
}
```

### CI Workflow Fix
```yaml
# Source: .github/workflows/benchmark.yml:38-41
# BEFORE (silent failure):
#   ./build/benchmarks/btop_bench_proc --json > bench_proc.json || true
#
# AFTER (explicit failure reporting):
- name: Run micro-benchmarks
  run: |
    ./build/benchmarks/btop_bench_strings --json > bench_strings.json
    ./build/benchmarks/btop_bench_draw --json > bench_draw.json
    ./build/benchmarks/btop_bench_proc --json > bench_proc.json
    # Validate proc benchmark produced data
    if ! python3 -c "import json,sys; d=json.load(open('bench_proc.json')); assert d.get('results',[]), 'No proc results'"; then
      echo "::warning::Proc benchmark produced no results"
    fi
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| std::ifstream for /proc reads | Tools::read_proc_file() POSIX read | Phase 3 (2026-02-27) | Eliminated ~1500-2000 heap allocs per update |
| Silent `\|\| true` for flaky benchmarks | Explicit failure detection in CI | Phase 7 (this phase) | Zero-data runs become visible |
| try/catch swallow of init failures | Structured skip reporting | Phase 7 (this phase) | CI logs show exactly what was skipped |

**Deprecated/outdated:**
- std::ifstream for /proc file reading in benchmarks: replaced by Tools::read_proc_file() in production code (Phase 3), benchmarks still use old pattern (this phase fixes it)

## Open Questions

1. **Should the /proc directory scan sub-benchmark also be updated?**
   - What we know: The `/proc directory scan` benchmark (lines 74-87) uses `std::filesystem::directory_iterator`. This was not flagged in the audit as a gap because Phase 3 did not replace directory scanning (it replaced per-PID file reads).
   - What's unclear: Whether this is worth changing for consistency
   - Recommendation: Leave as-is. It measures the directory enumeration overhead which is the same in both old and new code paths. Only file reads were optimized.

2. **Should unused includes be cleaned up?**
   - What we know: After replacing ifstream sub-benchmarks, `<fstream>` and `<sstream>` includes become unused in bench_proc_collect.cpp
   - What's unclear: Whether this is worth doing as part of this phase
   - Recommendation: Yes, clean up unused includes as part of the sub-benchmark update. Minimal effort, cleaner code.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 + nanobench v4.3.11 |
| Config file | tests/CMakeLists.txt, benchmarks/CMakeLists.txt |
| Quick run command | `cmake --build build --target btop_bench_proc && ./build/benchmarks/btop_bench_proc` |
| Full suite command | `cmake --build build && ctest --test-dir build --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PROF-02 | Sub-benchmarks use read_proc_file() | manual-only | Build + run bench_proc_collect on Linux, verify output includes read_proc_file timing | N/A -- benchmark output inspection |
| PROF-03 | CI detects zero-data proc benchmark runs | integration | Push to branch, observe CI workflow, verify proc failure is reported | N/A -- CI workflow test |
| IODC-01 | Benchmark measures optimized I/O path | unit (code review) | Verify bench_proc_collect.cpp calls Tools::read_proc_file, not ifstream | N/A -- code inspection |

**Justification for manual-only:** These requirements are about benchmark infrastructure correctness, not functional code behavior. Verification is: (1) code review confirms correct API calls, (2) benchmark compiles and runs on Linux, (3) CI workflow change is syntactically correct. No unit test can meaningfully validate "this benchmark measures the right code path."

### Sampling Rate
- **Per task commit:** `cmake --build build --target btop_bench_proc && ./build/benchmarks/btop_bench_proc` (Linux only)
- **Per wave merge:** Full build + all benchmarks
- **Phase gate:** Code review + Linux benchmark run + CI workflow syntax validation

### Wave 0 Gaps
None -- existing test/benchmark infrastructure covers all phase requirements. No new test files needed.

## Sources

### Primary (HIGH confidence)
- `benchmarks/bench_proc_collect.cpp` -- Direct inspection of current benchmark code
- `src/btop_tools.hpp:386-398` -- Tools::read_proc_file() implementation
- `.github/workflows/benchmark.yml` -- CI workflow with `|| true` on line 41
- `.planning/v1.0-MILESTONE-AUDIT.md` -- Integration gaps 1-2, broken flow 1
- `src/linux/btop_collect.cpp:287-337` -- Shared::init() implementation
- `benchmarks/convert_nanobench_json.py` -- JSON conversion with error handling

### Secondary (MEDIUM confidence)
- None needed -- all findings verified from primary source code inspection

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All components already exist in the project, no new dependencies
- Architecture: HIGH - Changes are localized to 2 files (bench_proc_collect.cpp, benchmark.yml)
- Pitfalls: HIGH - All pitfalls identified from direct code inspection of existing files

**Research date:** 2026-02-27
**Valid until:** No expiry -- findings are based on project source code, not external APIs
