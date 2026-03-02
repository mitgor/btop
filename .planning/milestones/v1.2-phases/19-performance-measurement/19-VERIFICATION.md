---
phase: 19-performance-measurement
status: passed
verified: 2026-03-01
requirements: [PERF-01, PERF-02]
---

# Phase 19: Performance Measurement — Verification

## Phase Goal

> Quantify the actual CPU and memory impact of v1.0+v1.1 optimizations with reproducible methodology

## Must-Have Verification

### 1. CPU usage measured with documented, reproducible methodology

**Status: PASSED**

- `docs/PERFORMANCE.md` contains per-cycle timing tables (wall, collect, draw) with mean and median values
- Methodology section documents: tool (btop --benchmark), workload (50 cycles/run, 5 warmup), duration (6 runs), environment (Apple M4 Max, macOS 26.3, clang 17.0.0)
- Build flags documented: `cmake -DCMAKE_BUILD_TYPE=Release -DBTOP_LTO=ON -DBTOP_GPU=ON`
- Results show: wall time -4.4% (1,724 -> 1,648 us), collect -6.3%, draw +19.2%

### 2. Memory footprint measured with documented, reproducible methodology

**Status: PASSED**

- Peak RSS measured via `os.wait4()` ru_maxrss (primary) and `/usr/bin/time -l` (cross-validation)
- Both methods agree: os.wait4 reports 13.4 MB, /usr/bin/time reports 13.2 MB (within 2%)
- Results show: +13.7% RSS increase (11.8 -> 13.4 MB), documented as deliberate trade-off for pre-allocated buffers

### 3. Before/after comparison with measured values

**Status: PASSED**

- Baseline: v1.4.6 upstream (with backported --benchmark flag)
- Current: HEAD at commit 656aded (post-v1.2)
- Both binaries built from source with identical cmake flags
- All values in PERFORMANCE.md are real measurements (verified against JSON data)

### 4. Results committed with methodology documentation

**Status: PASSED**

- `scripts/measure_performance.py` committed (b9cd3c3) — 476 lines, self-contained
- `docs/PERFORMANCE.md` committed (1ebf5c9) — 151 lines with full methodology
- `benchmark_results/v12_comparison_report.json` committed with structured data
- Raw per-binary data files committed
- Reproduction command documented: `python3 scripts/measure_performance.py`

## Requirement Traceability

| Requirement | Description | Status | Evidence |
|---|---|---|---|
| PERF-01 | CPU usage measured before/after with documented methodology | PASSED | docs/PERFORMANCE.md Per-Cycle Timing section |
| PERF-02 | Memory footprint measured before/after with documented methodology | PASSED | docs/PERFORMANCE.md Memory section |

## Artifact Verification

| Artifact | Required | Exists | Min Lines | Actual Lines | Contains |
|---|---|---|---|---|---|
| scripts/measure_performance.py | Yes | Yes | 100 | 476 | subprocess.run with --benchmark |
| docs/PERFORMANCE.md | Yes | Yes | 60 | 151 | "Methodology" section |
| benchmark_results/v12_comparison_report.json | Yes | Yes | -- | -- | old/new/changes keys |

## Summary

**Phase 19 PASSED** — All 4 must-haves verified. Both PERF-01 and PERF-02 requirements satisfied with real measured data, documented methodology, and reproducible script.
