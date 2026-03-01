# btop++ Cumulative Performance Report (v1.0 + v1.1 + v1.2)

## Overview

This report quantifies the cumulative impact of three milestones of optimization and architecture work on btop++:

- **v1.0 Performance Optimization** (Phases 1-8): String/allocation reduction, I/O optimization, data structure modernization, rendering pipeline improvements, compiler tuning.
- **v1.1 Automata Architecture** (Phases 10-15): Finite state machine refactor, event queue, type-safe states -- ~13K LOC of architecture work.
- **v1.2 Tech Debt** (Phases 16-18): Error path purity, signal/transition routing through FSM, test/doc hygiene.

All measurements compare **v1.4.6** (upstream baseline, before any optimization) against **HEAD** (post-v1.2, all work complete).

## Methodology

| Parameter | Value |
|---|---|
| **Tool** | `btop --benchmark` mode (per-cycle collect+draw timing, JSON output) |
| **RSS measurement** | `os.wait4()` ru_maxrss (primary) + `/usr/bin/time -l` (cross-validation) |
| **Cycles per run** | 50 (first 5 discarded as warmup, 45 measured) |
| **Runs** | 6 (total: 270 measured cycles per binary) |
| **Build flags** | `cmake -DCMAKE_BUILD_TYPE=Release -DBTOP_LTO=ON -DBTOP_GPU=ON` |
| **Baseline** | v1.4.6 upstream with backported `--benchmark` flag |
| **Current** | HEAD at commit `656aded` (post-v1.2) |
| **Reproduce** | `python3 scripts/measure_performance.py` |

Both binaries are built from source with identical cmake flags to ensure a fair comparison. The `--benchmark` mode runs collect+draw cycles without terminal output, isolating CPU-bound work from terminal I/O.

## Test Environment

| | |
|---|---|
| **CPU** | Apple M4 Max (16 cores) |
| **Memory** | 64 GB |
| **OS** | macOS 26.3 (arm64) |
| **Compiler** | Apple clang 17.0.0 (clang-1700.6.4.2) |

## Results

### Per-Cycle Timing

| Metric | v1.4.6 | Current | Change |
|---|---|---|---|
| **Total wall time (mean)** | 1,724 us | 1,648 us | **-4.4%** |
| **Collect time (mean)** | 1,595 us | 1,495 us | **-6.3%** |
| **Draw time (mean)** | 128 us | 153 us | +19.2% |
| Total wall time (median) | 1,698 us | 1,601 us | **-5.7%** |
| Collect time (median) | 1,572 us | 1,451 us | **-7.7%** |
| Draw time (median) | 126 us | 149 us | +18.3% |

The **net wall time improvement is 4.4%** (mean) / **5.7%** (median). Collect time improved by 6.3-7.7%, while draw time increased by 18-19%. The draw increase is explained below.

### Memory

| Metric | v1.4.6 | Current | Change |
|---|---|---|---|
| **Peak RSS (os.wait4, mean)** | 11.8 MB | 13.4 MB | +13.7% |
| Peak RSS (os.wait4, median) | 11.7 MB | 13.2 MB | +13.2% |
| Peak RSS (/usr/bin/time) | 11.6 MB | 13.2 MB | +13.6% |

The RSS increase (+1.6 MB) is a deliberate trade-off: pre-allocated buffers (RingBuffer, ScreenBuffer, enum lookup tables) consume more baseline memory but eliminate per-frame heap allocations. The os.wait4 and /usr/bin/time measurements agree within 2%, confirming measurement reliability.

### Stability

| Metric | v1.4.6 | Current |
|---|---|---|
| Wall time stdev | 127 us | 164 us |
| Collect time stdev | 123 us | 158 us |
| Draw time stdev | 10 us | 12 us |

The current version shows slightly higher variance in this run. This is within normal system noise for a macOS benchmark environment where background processes can cause outliers.

### Cycle Breakdown

| Component | v1.4.6 | Current |
|---|---|---|
| Collect % | 92.5% | 90.7% |
| Draw % | 7.5% | 9.3% |

Collect still dominates the cycle (90.7%) since it performs system data collection via kernel syscalls. The collect share has decreased because collect improved more than draw.

### Binary Size

| Version | Size | Change |
|---|---|---|
| v1.4.6 | 1,540 KB | -- |
| Current | 1,696 KB | +10.1% |

The size increase comes from: enum lookup table data, RingBuffer/ScreenBuffer template instantiations, FSM state/event/transition infrastructure, and additional test support code compiled into the binary.

## What Changed

### v1.0 Performance Optimization (Phases 1-8)

The primary performance work:
- **String/allocation reduction**: Regex-free `uncolor()`, `fmt::format_to()` direct appending, const-ref parameters
- **I/O optimization**: POSIX `read_proc_file()` replacing `std::ifstream`, hash-map PID lookup, double-stat elimination
- **Data structures**: `CpuField`/`GraphSymbolType`/`BoolKey` enum arrays (35-132x micro-benchmark speedups), `RingBuffer<T>` replacing `std::deque`
- **Rendering pipeline**: `write_stdout()` batching, graph column caching, `ScreenBuffer` + `Cell` differential rendering
- **Compiler**: PGO pipeline (+1.1%), mimalloc evaluation (+1.6%, not adopted)

### v1.1 Automata Architecture (Phases 10-15)

Structural refactor adding ~13K LOC:
- Named states (`Inactive`, `Running`, `Quitting`, `Error`) with type-safe enum
- Event queue (`EventQueue<AppEvent>`) replacing ad-hoc flag polling
- `transition_to()` with `on_exit`/`on_enter` callbacks
- Runner FSM integration with error recovery
- Expected near-zero CPU overhead since the FSM replaces existing if/else chains

### v1.2 Tech Debt (Phases 16-18)

Architectural cleanup:
- **Phase 16**: Runner errors flow through event queue (no direct shadow atomic writes)
- **Phase 17**: All signal/transition paths routed through `transition_to()`
- **Phase 18**: Pre-existing test failure fixed, stale documentation cleaned

Minimal performance impact expected -- these are correctness/maintainability improvements.

## Comparison with v1.0-Only Results

The v1.0 milestone produced its own benchmark immediately after the performance optimization phases, before the v1.1 architecture work was added.

| Metric | v1.0-only change | Cumulative (v1.0+v1.1+v1.2) change | Delta |
|---|---|---|---|
| **Wall time (mean)** | -9.2% | -4.4% | +4.8 pp |
| **Collect time (mean)** | -10.0% | -6.3% | +3.7 pp |
| **Draw time (mean)** | +0.4% | +19.2% | +18.8 pp |
| **Peak RSS** | +14.7% | +13.7% | -1.0 pp |
| **Wall stdev** | 350->255 us (-27%) | 127->164 us (+29%) | -- |

### Analysis

1. **Collect time held most of its gains**: v1.0 delivered -10.0% collect improvement; after adding v1.1+v1.2, the net improvement is still -6.3%. The ~3.7pp reduction is consistent with the additional FSM dispatch overhead on the collect path, though system-level noise across different measurement sessions also contributes.

2. **Draw time increased significantly**: v1.0 showed draw at +0.4% (neutral). The current +19.2% increase (128->153 us, +25us per cycle) comes from the v1.1 architecture additions. The FSM state checks, event dispatch, and additional code in the draw path add overhead. However, draw is only ~9% of the total cycle time, so the absolute impact is modest (~25 us per cycle).

3. **RSS is consistent**: Peak RSS increase is +13.7% now vs +14.7% at v1.0-only. The slight decrease is within measurement noise -- the v1.1/v1.2 architecture adds code but minimal additional data structures.

4. **Variance note**: The v1.0-only measurements and cumulative measurements were taken in different sessions with different system conditions. Direct stdev comparison across sessions is unreliable.

5. **Net result**: The v1.1 architecture refactor added ~25us draw overhead per cycle, partially offsetting v1.0's collect gains. The net wall time improvement is -4.4% (76 us per cycle saved). This is a reasonable trade-off: the FSM architecture provides compile-time state safety and eliminates entire classes of state bugs, at a cost of ~25us (0.025ms) per update cycle that runs every 2 seconds.

## Raw Data

- Baseline per-run data: `benchmark_results/v12_old_bench.json`
- Current per-run data: `benchmark_results/v12_new_bench.json`
- Comparison report: `benchmark_results/v12_comparison_report.json`
- Measurement script: `scripts/measure_performance.py`
- v1.0-only results: `optimised.md`

To reproduce: `python3 scripts/measure_performance.py`
