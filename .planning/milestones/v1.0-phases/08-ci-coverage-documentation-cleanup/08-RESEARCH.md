# Phase 8: CI Coverage & Documentation Cleanup - Research

**Researched:** 2026-02-27
**Domain:** GitHub Actions CI workflows, nanobench JSON integration, PGO build validation
**Confidence:** HIGH

## Summary

Phase 8 closes three specific gaps identified in the v1.0 milestone audit: (1) `btop_bench_ds` is built by CMake but never invoked in the benchmark CI workflow, leaving DATA-01/DATA-02 regressions undetected; (2) the PGO build path (`scripts/pgo-build.sh`) has no CI validation, meaning breakage would go unnoticed; (3) the ROADMAP.md has uncommitted checkbox updates and needs Phase 8's own status reflected.

All three gaps are well-scoped and low-risk. The benchmark integration requires one non-trivial compatibility fix: `bench_data_structures.cpp` outputs an **array** of 4 nanobench JSON objects (`[{results:...}, {results:...}, ...]`), while the existing `convert_nanobench_json.py` expects a single object with a top-level `"results"` key. The converter must be updated to handle arrays. The PGO CI job is straightforward -- the existing `scripts/pgo-build.sh` handles the full workflow and just needs to be invoked in a CI context with Clang on Linux. The ROADMAP update is a documentation task with uncommitted changes already staged.

**Primary recommendation:** Add `btop_bench_ds` invocation to `benchmark.yml`, update `convert_nanobench_json.py` to handle array-of-nanobench-objects format, add a scheduled PGO validation job to `cmake-linux.yml`, and commit the ROADMAP progress updates.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PROF-02 | Micro-benchmarks for hot functions (nanobench) | btop_bench_ds in CI strengthens regression detection for data structure micro-benchmarks |
| PROF-03 | CI regression detection pipeline | Adding btop_bench_ds to benchmark.yml closes the last CI blind spot |
| DATA-01 | Enum-indexed flat arrays replacing unordered_map | btop_bench_ds benchmarks measure enum-array vs unordered_map -- CI detection prevents regression |
| DATA-02 | Ring buffers replacing deque for time-series | btop_bench_ds benchmarks measure RingBuffer vs deque -- CI detection prevents regression |
</phase_requirements>

## Standard Stack

### Core

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| GitHub Actions | N/A | CI/CD platform | Already used by all existing workflows |
| benchmark-action/github-action-benchmark | v1 | Benchmark result storage and alerting | Already integrated in benchmark.yml |
| nanobench | v4.3.11 | C++ micro-benchmark framework | Already used by all 4 benchmark binaries |
| convert_nanobench_json.py | Custom | Format converter: nanobench JSON -> customSmallerIsBetter | Already exists in benchmarks/ directory |
| pgo-build.sh | Custom | PGO workflow automation script | Already exists in scripts/ directory |

### Supporting

| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| Clang | 19+ | Compiler for PGO CI job | Required for PGO profile generation and use (llvm-profdata) |
| Ninja | Latest | CMake generator | Used by all CI build jobs |
| Python 3 | System | Run convert_nanobench_json.py | Already available on ubuntu-24.04 runners |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Scheduled PGO CI job | PGO on every push | Too expensive (4-step build process); scheduled weekly is sufficient for validation |
| Fixing bench_ds JSON format | Fixing converter to handle arrays | Converter fix is simpler and doesn't change benchmark binary behavior |

## Architecture Patterns

### Pattern 1: Benchmark CI Integration (Existing Pattern)

**What:** Each benchmark binary is invoked with `--json`, output captured to a file, all files fed to `convert_nanobench_json.py`, combined output stored via `github-action-benchmark`.

**When to use:** Adding any new benchmark to CI.

**Example (from existing benchmark.yml):**
```yaml
- name: Run micro-benchmarks
  run: |
    ./build/benchmarks/btop_bench_strings --json > bench_strings.json
    ./build/benchmarks/btop_bench_draw --json > bench_draw.json
    # ... add new benchmarks here

- name: Convert results
  run: |
    python3 benchmarks/convert_nanobench_json.py \
      bench_strings.json bench_draw.json ... \
      > benchmark_results.json
```

### Pattern 2: Scheduled CI Validation (For PGO)

**What:** Use `schedule` trigger with cron expression to run expensive/periodic validation jobs that don't need to run on every push.

**When to use:** Validating build infrastructure (PGO, release builds) where breakage detection within a few days is acceptable.

**Example:**
```yaml
on:
  schedule:
    - cron: '0 6 * * 1'  # Weekly on Monday at 6 AM UTC
  workflow_dispatch:  # Allow manual triggering
```

### Pattern 3: Error Handling for Non-Critical Benchmarks (Existing Pattern from Phase 7)

**What:** Use `if !` conditional to catch benchmark failures, emit `::warning::` annotations, and write empty JSON to prevent downstream converter failures.

**When to use:** Benchmarks that may fail in certain environments but should not block CI.

**Example (from existing benchmark.yml for proc benchmark):**
```yaml
if ! ./build/benchmarks/btop_bench_proc --json > bench_proc.json; then
  echo "::warning::Proc benchmark crashed or failed to run"
  echo '{"results":[]}' > bench_proc.json
fi
```

### Anti-Patterns to Avoid

- **Silent failure with `|| true`:** Phase 7 explicitly removed this. Never use `|| true` for benchmark invocations -- use explicit error handling with `::warning::`.
- **Running PGO on every push:** PGO is a 4-step process (configure + build + train + rebuild). Too expensive for CI on every commit. Use scheduled runs.
- **Modifying benchmark binary JSON output format:** The benchmark binaries work correctly. Fix the converter to accept their format, not vice versa.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Benchmark result storage | Custom JSON archival | benchmark-action/github-action-benchmark@v1 | Already integrated, handles alerting, PR comments, historical tracking |
| PGO build orchestration | New CI-specific PGO script | Existing `scripts/pgo-build.sh` | Script already handles Clang/GCC detection, profraw merging, error checking |
| JSON format conversion | Inline jq or shell parsing | `convert_nanobench_json.py` (with array support) | Already handles nanobench and btop benchmark formats; extend, don't replace |

**Key insight:** All infrastructure already exists. Phase 8 is about wiring existing components together and fixing one format compatibility issue.

## Common Pitfalls

### Pitfall 1: bench_data_structures JSON Format Mismatch

**What goes wrong:** `btop_bench_ds --json` outputs `[{results:...}, {results:...}, {results:...}, {results:...}]` (array of 4 nanobench JSON objects, one per bench group). The converter expects a single `{results: [...]}` object.

**Why it happens:** The other benchmarks (strings, draw, proc) each have one `ankerl::nanobench::Bench` instance and render a single JSON object. `bench_data_structures.cpp` has 4 separate `Bench` instances (CPU fields, graph symbols, config bools, ring buffer) and wraps them in a JSON array at lines 258-266.

**How to avoid:** Update `convert_nanobench_json.py` to detect when input is a JSON array. If array, iterate elements and call `convert_nanobench()` on each element that has a `"results"` key.

**Warning signs:** `::warning::Unknown format in bench_ds.json` in CI logs.

### Pitfall 2: PGO CI Job Failing on Profile Path

**What goes wrong:** `pgo-build.sh` uses `find . -name "*.profraw"` which can find stale profiles from previous runs, or the profile path may differ between local and CI environments.

**Why it happens:** CI runners are ephemeral (clean workspace), so this is less of an issue than locally. But the script's `find` patterns should still work correctly.

**How to avoid:** The script already has good error handling (checks for empty `PROFRAW_FILES`, exits on failure). In CI, just ensure `CXX` environment variable is set to a Clang compiler.

**Warning signs:** "ERROR: No .profraw files found" in CI log output.

### Pitfall 3: PGO Job Resource Consumption

**What goes wrong:** PGO requires two full CMake configure+build cycles plus a training run. This is ~3-4x the cost of a normal build.

**Why it happens:** PGO by definition requires instrumenting, training, and rebuilding.

**How to avoid:** Use `schedule` trigger (weekly), not `push`/`pull_request`. Add `workflow_dispatch` for manual triggering. Keep `--benchmark 50` training run (matches existing script).

**Warning signs:** CI billing alerts if run on every push.

### Pitfall 4: ROADMAP Uncommitted Changes

**What goes wrong:** The ROADMAP already has local uncommitted changes (checkboxes for phases 3.1, 4, 5, 6 marked complete). These must be included in the commit along with Phase 8's own completion status.

**Why it happens:** Previous phase execution updated the working tree but didn't commit the ROADMAP checkbox changes separately.

**How to avoid:** Ensure the ROADMAP update task captures ALL pending changes (both the already-modified checkboxes and the Phase 8 status update).

**Warning signs:** `git diff .planning/ROADMAP.md` showing changes before Phase 8 work begins.

## Code Examples

### Example 1: Adding btop_bench_ds to benchmark.yml (Linux Job)

```yaml
# In the "Run micro-benchmarks" step, add after existing benchmarks:
./build/benchmarks/btop_bench_ds --json > bench_ds.json

# In the "Convert results" step, add bench_ds.json to the argument list:
python3 benchmarks/convert_nanobench_json.py \
  bench_strings.json bench_draw.json bench_proc.json bench_ds.json bench_macro.json \
  > benchmark_results.json
```

### Example 2: Updating convert_nanobench_json.py to Handle Array Format

```python
# In the main() function, after loading JSON data:
if isinstance(data, list):
    # bench_data_structures outputs array of nanobench objects
    for item in data:
        if isinstance(item, dict) and "results" in item:
            all_entries.extend(convert_nanobench(item))
elif "results" in data:
    all_entries.extend(convert_nanobench(data))
elif "benchmark" in data:
    all_entries.extend(convert_btop_benchmark(data))
else:
    print(f"Warning: Unknown format in {path}", file=sys.stderr)
```

### Example 3: PGO Validation Job (Scheduled)

```yaml
pgo-validate:
  name: pgo-build-validation
  runs-on: ubuntu-24.04
  steps:
    - uses: actions/checkout@v6

    - name: Install Clang 19
      run: wget -qO - https://apt.llvm.org/llvm.sh | sudo bash -s -- 19 all

    - name: Install Ninja
      run: sudo apt-get install -y ninja-build

    - name: Run PGO build script
      env:
        CXX: clang++-19
      run: ./scripts/pgo-build.sh Release

    - name: Verify PGO binary exists and runs
      run: |
        test -f build-pgo-use/btop
        ./build-pgo-use/btop --benchmark 5
```

### Example 4: Adding btop_bench_ds to macOS Benchmark Job

```yaml
# macOS job already runs bench_strings and bench_draw but not proc (Linux-only)
# btop_bench_ds is cross-platform (self-contained, no /proc dependency)
- name: Run micro-benchmarks
  run: |
    ./build/benchmarks/btop_bench_strings --json > bench_strings.json
    ./build/benchmarks/btop_bench_draw --json > bench_draw.json
    ./build/benchmarks/btop_bench_ds --json > bench_ds.json

- name: Convert results
  run: |
    python3 benchmarks/convert_nanobench_json.py \
      bench_strings.json bench_draw.json bench_ds.json bench_macro.json \
      > benchmark_results.json
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `\|\| true` for benchmark failures | `if !` with `::warning::` annotations | Phase 7 | CI visibility for failures |
| Single nanobench Bench per binary | Multiple Bench instances in bench_ds | Phase 4 | Requires converter update for array format |
| Manual PGO via pgo-build.sh | Still manual | Phase 6 | Phase 8 adds CI validation |

## Open Questions

1. **PGO Job Placement: Separate Workflow vs cmake-linux.yml Job**
   - What we know: PGO validation is a periodic check, not per-commit. cmake-linux.yml already has sanitizer jobs.
   - What's unclear: Whether to add a `pgo-validate` job to cmake-linux.yml (with schedule trigger) or create a separate workflow file.
   - Recommendation: Add to cmake-linux.yml as a new job with `schedule` trigger on the workflow level. This is simpler and keeps all Linux CI jobs together. The schedule trigger only fires on the default branch, so it won't pollute PR builds. However, if `cmake-linux.yml` already has path filters that would prevent schedule-triggered runs from including the right files, a separate workflow may be cleaner. **Research finding:** The existing `cmake-linux.yml` has `paths:` filters under `push:` and `pull_request:`, but `schedule:` triggers ignore path filters (they always run). A `schedule` trigger added to `cmake-linux.yml` would run ALL jobs in the workflow (build matrix + sanitizer + pgo-validate), which is wasteful. **Recommendation: Create a separate `pgo-validate.yml` workflow** with only the PGO job, triggered on `schedule` + `workflow_dispatch`.

2. **btop_bench_ds on macOS CI**
   - What we know: `btop_bench_ds` is self-contained (no /proc dependency), so it should work on macOS.
   - What's unclear: Whether to add it to the macOS benchmark job too.
   - Recommendation: Yes, add to macOS job. The CMakeLists.txt builds `btop_bench_ds` unconditionally (not gated by `if(LINUX)` like `btop_bench_proc`). Cross-platform coverage strengthens DATA-01/DATA-02 regression detection.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | CTest + nanobench v4.3.11 |
| Config file | tests/CMakeLists.txt, benchmarks/CMakeLists.txt |
| Quick run command | `ctest --test-dir build` |
| Full suite command | `cmake --build build && ctest --test-dir build && ./build/benchmarks/btop_bench_ds --json > /dev/null` |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PROF-02 | btop_bench_ds produces valid JSON | smoke | `./build/benchmarks/btop_bench_ds --json \| python3 -c "import json,sys; json.load(sys.stdin)"` | binary exists, JSON validation is new |
| PROF-03 | benchmark.yml invokes btop_bench_ds | integration | Verify in workflow YAML (static check) | benchmark.yml exists |
| DATA-01 | Enum-array benchmarks in CI output | integration | Check benchmark_results.json contains ds entries | Part of CI pipeline |
| DATA-02 | RingBuffer benchmarks in CI output | integration | Check benchmark_results.json contains ds entries | Part of CI pipeline |

### Sampling Rate

- **Per task commit:** Build + `ctest --test-dir build` + local `btop_bench_ds --json` smoke test
- **Per wave merge:** Full benchmark suite locally
- **Phase gate:** Verify benchmark.yml changes produce expected output via dry-run analysis

### Wave 0 Gaps

- [ ] `convert_nanobench_json.py` array format support -- must be added before btop_bench_ds can be integrated
- [ ] `pgo-validate.yml` workflow -- does not exist yet, must be created

## Sources

### Primary (HIGH confidence)

- `.github/workflows/benchmark.yml` -- Existing benchmark CI workflow structure, steps, and github-action-benchmark integration
- `benchmarks/bench_data_structures.cpp` -- JSON output format (array of 4 nanobench objects at lines 258-266)
- `benchmarks/convert_nanobench_json.py` -- Converter logic expects `"results"` key (line 71)
- `benchmarks/CMakeLists.txt` -- `btop_bench_ds` target definition (line 20-22), unconditional (not gated by `if(LINUX)`)
- `scripts/pgo-build.sh` -- Full PGO workflow script with Clang/GCC support
- `.planning/v1.0-MILESTONE-AUDIT.md` -- Identifies exact gaps (btop_bench_ds CI blind spot, PGO CI gap, stale ROADMAP)
- `CMakeLists.txt` -- PGO CMake options (BTOP_PGO_GENERATE, BTOP_PGO_USE)
- `.github/workflows/cmake-linux.yml` -- Existing sanitizer job pattern, path filters that affect schedule triggers
- nanobench v4.3.11 `templates::json()` format confirmed via header inspection: `{"results": [...]}`

### Secondary (MEDIUM confidence)

- [benchmark-action/github-action-benchmark](https://github.com/benchmark-action/github-action-benchmark) -- customSmallerIsBetter format requires `name`, `unit`, `value` fields in a JSON array
- GitHub Actions documentation -- `schedule` triggers ignore `paths:` filters and run all jobs in a workflow

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All tools already in use, no new dependencies
- Architecture: HIGH - Extending existing patterns (benchmark invocation, converter, CI jobs)
- Pitfalls: HIGH - JSON format mismatch verified by reading source code; PGO CI constraints understood from cmake-linux.yml structure

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable infrastructure, no fast-moving dependencies)
