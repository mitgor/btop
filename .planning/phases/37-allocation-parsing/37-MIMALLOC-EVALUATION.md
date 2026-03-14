# mimalloc Evaluation -- Phase 37 (MEM-04)

## Objective

Evaluate whether replacing the system default allocator with [mimalloc](https://github.com/microsoft/mimalloc) yields a meaningful performance improvement for btop. The adoption threshold is a **3% or greater** improvement in the benchmark suite.

## Methodology

- **Tool:** [hyperfine](https://github.com/sharkdp/hyperfine) with 11 runs per configuration
- **Build:** Release mode (`-O2`), same compiler, same machine, same flags except allocator linkage
- **Configurations:**
  1. `default-allocator` -- system libc malloc (macOS libmalloc)
  2. `mimalloc` -- linked with `mimalloc-static` via CMake `target_link_libraries`
- **Workload:** btop benchmark mode (`--benchmark`), exercising collection, sorting, drawing, and config paths
- **Raw data:** `mimalloc_benchmark.json` in project root

## Results

| Metric              | Default Allocator | mimalloc       |
|---------------------|-------------------|----------------|
| Mean (s)            | 0.25482           | 0.25075        |
| Median (s)          | 0.25403           | 0.24899        |
| Std Dev (s)         | 0.00624           | 0.00548        |
| Min (s)             | 0.24794           | 0.24303        |
| Max (s)             | 0.26738           | 0.26164        |
| Memory (bytes)      | 14,712,832        | 14,712,832     |
| Runs                | 11                | 11             |

**Improvement: 1.6%** ((0.25482 - 0.25075) / 0.25482 = 0.01597)

Memory usage was identical across both configurations.

## Decision

**NOT adopted as default.** The 1.6% improvement is below the 3% adoption threshold.

## Rationale

1. **Sub-threshold gain.** A 1.6% improvement (~4 ms per benchmark cycle) falls within measurement noise and does not justify adding a mandatory runtime dependency.

2. **No memory benefit.** Both configurations used identical RSS (14.7 MB), so mimalloc's arena-based allocation does not reduce btop's memory footprint.

3. **Dependency cost.** Requiring mimalloc at build time adds a system dependency that many package managers may not provide by default, increasing friction for source builds.

4. **Opt-in preserved.** A `BTOP_MIMALLOC` CMake option (OFF by default) is provided for users who compile from source and have mimalloc installed, allowing them to opt in if they value the marginal improvement.

## Availability

Users who want mimalloc can enable it at configure time:

```bash
cmake -B build -DBTOP_MIMALLOC=ON
cmake --build build
```

This requires mimalloc to be installed and discoverable by CMake's `find_package`.

## References

- Raw benchmark data: `mimalloc_benchmark.json`
- CMake option: `BTOP_MIMALLOC` in `CMakeLists.txt`
- Requirement: MEM-04 (Evaluate alternative allocator)
