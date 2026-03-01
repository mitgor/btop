#!/usr/bin/env python3
"""
btop++ Cumulative Performance Measurement (v1.0 + v1.1 + v1.2)

Measures CPU timing and memory usage for:
  - Baseline: v1.4.6 upstream (with backported --benchmark flag)
  - Current:  HEAD (post-v1.2, all optimizations + architecture + tech debt)

Methodology:
  - Internal: btop --benchmark mode, 50 cycles/run, 5 warmup discarded, 6 runs
  - External: os.wait4() for peak RSS (ru_maxrss), /usr/bin/time -l cross-validation
  - Builds both binaries from source with identical cmake flags

Usage:
  python3 scripts/measure_performance.py

Output:
  - benchmark_results/v12_old_bench.json   (baseline raw data)
  - benchmark_results/v12_new_bench.json   (current raw data)
  - benchmark_results/v12_comparison_report.json (comparison)
  - stdout: human-readable comparison table
"""

import json
import os
import re
import signal
import statistics
import subprocess
import sys
import time

# ─── Configuration ────────────────────────────────────────────────────────────

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTS_DIR = os.path.join(REPO_ROOT, "benchmark_results")

# Baseline: v1.4.6 with backported --benchmark flag
BASELINE_WORKTREE = "/tmp/btop-old"
BASELINE_BINARY = os.path.join(BASELINE_WORKTREE, "build-bench", "btop")

# Current: HEAD build
CURRENT_BUILD_DIR = os.path.join(REPO_ROOT, "build-perf")
CURRENT_BINARY = os.path.join(CURRENT_BUILD_DIR, "btop")

# Benchmark parameters
BENCH_CYCLES = 50       # cycles per --benchmark invocation
WARMUP_CYCLES = 5       # first N cycles discarded (cold-start effects)
RUNS = 6                # repeat entire benchmark N times for stability
MEASURED_PER_RUN = BENCH_CYCLES - WARMUP_CYCLES  # 45 warm cycles per run
TOTAL_MEASURED = RUNS * MEASURED_PER_RUN          # 270 total measured cycles

# Identical cmake flags for fair comparison
CMAKE_FLAGS = [
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBTOP_LTO=ON",
    "-DBTOP_GPU=ON",
]

# ─── Terminal colors ──────────────────────────────────────────────────────────

BOLD = "\033[1m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
BLUE = "\033[34m"
CYAN = "\033[36m"
NC = "\033[0m"


# ─── System info ──────────────────────────────────────────────────────────────

def get_system_info():
    """Collect system information for reproducibility."""
    info = {}
    info["platform"] = subprocess.getoutput("uname -m")
    info["os"] = (subprocess.getoutput("sw_vers -productName") + " " +
                  subprocess.getoutput("sw_vers -productVersion"))
    info["compiler"] = subprocess.getoutput("clang++ --version").split("\n")[0]
    info["cpu"] = subprocess.getoutput("sysctl -n machdep.cpu.brand_string")
    info["cores"] = subprocess.getoutput("sysctl -n hw.ncpu")
    info["memory_gb"] = round(
        int(subprocess.getoutput("sysctl -n hw.memsize")) / (1024**3), 1
    )
    return info


# ─── Build helpers ────────────────────────────────────────────────────────────

def build_binary(source_dir, build_dir, label):
    """Build btop from source with standard cmake flags."""
    print(f"  {YELLOW}Building {label} at {build_dir}...{NC}")

    cmake_cmd = ["cmake", "-B", build_dir, "-S", source_dir] + CMAKE_FLAGS
    result = subprocess.run(cmake_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  {RED}cmake configure failed:{NC}")
        print(result.stderr)
        sys.exit(1)

    build_cmd = ["cmake", "--build", build_dir, "-j"]
    result = subprocess.run(build_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  {RED}cmake build failed:{NC}")
        print(result.stderr)
        sys.exit(1)

    binary = os.path.join(build_dir, "btop")
    if not os.path.isfile(binary):
        print(f"  {RED}Binary not found at {binary}{NC}")
        sys.exit(1)

    print(f"  {GREEN}Built: {binary} ({os.path.getsize(binary) / 1024:.0f} KB){NC}")
    return binary


def ensure_baseline():
    """Ensure baseline v1.4.6 binary exists with --benchmark support."""
    if os.path.isfile(BASELINE_BINARY):
        # Verify it runs
        try:
            result = subprocess.run(
                [BASELINE_BINARY, "--version"],
                capture_output=True, text=True, timeout=5
            )
            if "1.4.6" in result.stdout or "1.4.6" in result.stderr:
                print(f"  {GREEN}Baseline binary OK: {BASELINE_BINARY}{NC}")
                return BASELINE_BINARY
        except Exception:
            pass

    # Binary missing or invalid
    print(f"  {RED}Baseline binary not found at {BASELINE_BINARY}{NC}")
    print(f"  {YELLOW}The v1.4.6 baseline requires a git worktree with the{NC}")
    print(f"  {YELLOW}--benchmark backport. Set up with:{NC}")
    print(f"    git worktree add /tmp/btop-old v1.4.6")
    print(f"    # Then apply --benchmark backport to btop.cpp, btop_cli.cpp, btop_cli.hpp")
    print(f"    # Then: cmake -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release \\")
    print(f"    #   -DBTOP_LTO=ON -DBTOP_GPU=ON && cmake --build build-bench")
    sys.exit(1)


def ensure_current():
    """Build current HEAD if binary doesn't exist."""
    if os.path.isfile(CURRENT_BINARY):
        print(f"  {GREEN}Current binary OK: {CURRENT_BINARY}{NC}")
        return CURRENT_BINARY

    return build_binary(REPO_ROOT, CURRENT_BUILD_DIR, "Current HEAD")


# ─── Benchmark runners ────────────────────────────────────────────────────────

def run_benchmark_internal(binary, cycles):
    """Run btop --benchmark and capture JSON timing data from stdout."""
    result = subprocess.run(
        [binary, "--benchmark", str(cycles)],
        capture_output=True, text=True, timeout=120
    )
    if result.returncode != 0:
        raise RuntimeError(f"Benchmark failed (exit {result.returncode}): {result.stderr}")

    return json.loads(result.stdout)


def measure_peak_rss(binary, cycles):
    """Measure peak RSS via fork + os.wait4() for ru_maxrss."""
    pid = os.fork()
    if pid == 0:
        # Child: redirect stdout/stderr to /dev/null, exec btop
        devnull = os.open(os.devnull, os.O_WRONLY)
        os.dup2(devnull, 1)
        os.dup2(devnull, 2)
        os.close(devnull)
        os.execv(binary, [binary, "--benchmark", str(cycles)])
        os._exit(1)

    # Parent: wait for child and collect rusage
    _, _, rusage = os.wait4(pid, 0)
    return rusage.ru_maxrss  # bytes on macOS


def measure_rss_via_time(binary, cycles):
    """Cross-validate peak RSS via /usr/bin/time -l."""
    result = subprocess.run(
        ["/usr/bin/time", "-l", binary, "--benchmark", str(cycles)],
        capture_output=True, text=True, timeout=120
    )
    # /usr/bin/time writes to stderr
    stderr = result.stderr
    # Look for "maximum resident set size" (macOS format)
    match = re.search(r"(\d+)\s+maximum resident set size", stderr)
    if match:
        return int(match.group(1))  # bytes on macOS
    return None


# ─── Statistics ───────────────────────────────────────────────────────────────

def summarize(values):
    """Compute summary statistics for a list of values."""
    if not values:
        return {"mean": 0, "median": 0, "stdev": 0, "min": 0, "max": 0, "n": 0}
    return {
        "mean": statistics.mean(values),
        "median": statistics.median(values),
        "stdev": statistics.stdev(values) if len(values) > 1 else 0,
        "min": min(values),
        "max": max(values),
        "n": len(values),
    }


def pct_change(old, new):
    """Calculate percentage change from old to new."""
    if old == 0:
        return 0
    return ((new - old) / old) * 100


def fmt_change(pct, lower_is_better=True):
    """Format a percentage change with color."""
    if lower_is_better:
        if pct < -2:
            return f"{GREEN}{pct:+.1f}%{NC}"
        elif pct > 2:
            return f"{RED}{pct:+.1f}%{NC}"
    return f"{pct:+.1f}%"


# ─── Full benchmark suite ─────────────────────────────────────────────────────

def run_suite(binary, label):
    """Run RUNS iterations of --benchmark, aggregate warm cycles + RSS."""
    print(f"  {YELLOW}{label}{NC}")

    all_wall = []
    all_collect = []
    all_draw = []
    all_rss = []

    for run in range(RUNS):
        # Internal timing via --benchmark JSON
        bench_data = run_benchmark_internal(binary, BENCH_CYCLES)
        timings = bench_data["benchmark"]["timings"]

        # Discard warmup cycles
        warm = timings[WARMUP_CYCLES:]
        walls = [t["wall_us"] for t in warm]
        collects = [t["collect_us"] for t in warm]
        draws = [t["draw_us"] for t in warm]

        all_wall.extend(walls)
        all_collect.extend(collects)
        all_draw.extend(draws)

        # External RSS via os.wait4
        rss_bytes = measure_peak_rss(binary, BENCH_CYCLES)
        rss_mb = rss_bytes / (1024 * 1024)
        all_rss.append(rss_mb)

        print(f"    Run {run + 1}/{RUNS}: wall={statistics.mean(walls):,.0f}us "
              f"collect={statistics.mean(collects):,.0f}us "
              f"draw={statistics.mean(draws):,.0f}us "
              f"rss={rss_mb:.1f}MB")

    # Cross-validate RSS with /usr/bin/time -l (single run)
    time_rss = measure_rss_via_time(binary, BENCH_CYCLES)
    time_rss_mb = time_rss / (1024 * 1024) if time_rss else None

    print(f"    /usr/bin/time RSS: {time_rss_mb:.1f}MB" if time_rss_mb else
          f"    /usr/bin/time RSS: unavailable")
    print()

    return {
        "wall_us": summarize(all_wall),
        "collect_us": summarize(all_collect),
        "draw_us": summarize(all_draw),
        "peak_rss_mb": summarize(all_rss),
        "time_rss_mb": time_rss_mb,
    }


# ─── Report generation ────────────────────────────────────────────────────────

def print_comparison(old, new, old_binary, new_binary):
    """Print human-readable comparison table to stdout."""
    print(f"{BOLD}{'=' * 70}{NC}")
    print(f"{BOLD}  COMPARISON RESULTS  (lower is better for timing metrics){NC}")
    print(f"{BOLD}{'=' * 70}{NC}")
    print()

    def row(label, old_val, new_val, unit, decimals=0, lower_is_better=True):
        pct = pct_change(old_val, new_val)
        change = fmt_change(pct, lower_is_better)
        fmt_s = f",.{decimals}f"
        o = f"{old_val:{fmt_s}}{unit}"
        n = f"{new_val:{fmt_s}}{unit}"
        print(f"  {label:<32} {o:>16} {n:>16} {change}")

    header = f"  {'Metric':<32} {'v1.4.6':>16} {'Current':>16} {'Change':>10}"
    sep = f"  {'-' * 32} {'-' * 16} {'-' * 16} {'-' * 10}"
    print(header)
    print(sep)

    # Per-cycle timing (means)
    print(f"  {CYAN}Per-cycle timing means (us):{NC}")
    row("  Total wall time", old["wall_us"]["mean"], new["wall_us"]["mean"], " us")
    row("  Collect time", old["collect_us"]["mean"], new["collect_us"]["mean"], " us")
    row("  Draw time", old["draw_us"]["mean"], new["draw_us"]["mean"], " us")

    # Medians
    print()
    print(f"  {CYAN}Per-cycle timing medians (us):{NC}")
    row("  Wall (median)", old["wall_us"]["median"], new["wall_us"]["median"], " us")
    row("  Collect (median)", old["collect_us"]["median"], new["collect_us"]["median"], " us")
    row("  Draw (median)", old["draw_us"]["median"], new["draw_us"]["median"], " us")

    # Memory
    print()
    print(f"  {CYAN}Memory:{NC}")
    row("  Peak RSS (os.wait4)", old["peak_rss_mb"]["mean"], new["peak_rss_mb"]["mean"],
        " MB", decimals=1)
    if old.get("time_rss_mb") and new.get("time_rss_mb"):
        row("  Peak RSS (/usr/bin/time)", old["time_rss_mb"], new["time_rss_mb"],
            " MB", decimals=1)

    # Variability
    print()
    print(f"  {CYAN}Variability (stdev):{NC}")
    print(f"  {'  Wall time':<32} {old['wall_us']['stdev']:>14,.0f} us "
          f"{new['wall_us']['stdev']:>14,.0f} us")
    print(f"  {'  Collect time':<32} {old['collect_us']['stdev']:>14,.0f} us "
          f"{new['collect_us']['stdev']:>14,.0f} us")
    print(f"  {'  Draw time':<32} {old['draw_us']['stdev']:>14,.0f} us "
          f"{new['draw_us']['stdev']:>14,.0f} us")

    # Cycle breakdown
    print()
    if old["wall_us"]["mean"] > 0 and new["wall_us"]["mean"] > 0:
        old_collect_pct = old["collect_us"]["mean"] / old["wall_us"]["mean"] * 100
        old_draw_pct = old["draw_us"]["mean"] / old["wall_us"]["mean"] * 100
        new_collect_pct = new["collect_us"]["mean"] / new["wall_us"]["mean"] * 100
        new_draw_pct = new["draw_us"]["mean"] / new["wall_us"]["mean"] * 100
        print(f"  {CYAN}Cycle breakdown:{NC}")
        print(f"  {'  Collect %':<32} {old_collect_pct:>15.1f}% {new_collect_pct:>15.1f}%")
        print(f"  {'  Draw %':<32} {old_draw_pct:>15.1f}% {new_draw_pct:>15.1f}%")

    # Binary sizes
    print()
    old_size = os.path.getsize(old_binary)
    new_size = os.path.getsize(new_binary)
    size_pct = pct_change(old_size, new_size)
    print(f"  {CYAN}Binary size:{NC}")
    print(f"    v1.4.6:  {old_size / 1024:,.0f} KB")
    print(f"    Current: {new_size / 1024:,.0f} KB ({size_pct:+.1f}%)")

    # Summary
    print()
    wall_pct = pct_change(old["wall_us"]["mean"], new["wall_us"]["mean"])
    collect_pct = pct_change(old["collect_us"]["mean"], new["collect_us"]["mean"])
    draw_pct = pct_change(old["draw_us"]["mean"], new["draw_us"]["mean"])
    rss_pct = pct_change(old["peak_rss_mb"]["mean"], new["peak_rss_mb"]["mean"])

    print(f"{BOLD}  SUMMARY:{NC}")
    print(f"    Wall time:    {fmt_change(wall_pct)} "
          f"({old['wall_us']['mean']:,.0f} -> {new['wall_us']['mean']:,.0f} us)")
    print(f"    Collect time: {fmt_change(collect_pct)} "
          f"({old['collect_us']['mean']:,.0f} -> {new['collect_us']['mean']:,.0f} us)")
    print(f"    Draw time:    {fmt_change(draw_pct)} "
          f"({old['draw_us']['mean']:,.0f} -> {new['draw_us']['mean']:,.0f} us)")
    print(f"    Peak memory:  {fmt_change(rss_pct)} "
          f"({old['peak_rss_mb']['mean']:.1f} -> {new['peak_rss_mb']['mean']:.1f} MB)")

    print()
    print(f"{BOLD}{'=' * 70}{NC}")
    print(f"  Raw data: {RESULTS_DIR}/v12_*.json")
    print(f"{BOLD}{'=' * 70}{NC}")

    return {
        "wall_pct": round(wall_pct, 2),
        "collect_pct": round(collect_pct, 2),
        "draw_pct": round(draw_pct, 2),
        "rss_pct": round(rss_pct, 2),
        "old_binary_kb": round(old_size / 1024, 0),
        "new_binary_kb": round(new_size / 1024, 0),
        "binary_size_pct": round(size_pct, 2),
    }


# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    os.makedirs(RESULTS_DIR, exist_ok=True)
    sys_info = get_system_info()

    print(f"{BOLD}{'=' * 70}{NC}")
    print(f"{BOLD}  btop++ Cumulative Performance Measurement{NC}")
    print(f"{BOLD}  Baseline: v1.4.6  |  Current: HEAD (post-v1.2){NC}")
    print(f"{BOLD}{'=' * 70}{NC}")
    print()
    print(f"  Platform:    {sys_info['platform']} / {sys_info['os']}")
    print(f"  Compiler:    {sys_info['compiler']}")
    print(f"  CPU:         {sys_info['cpu']}")
    print(f"  Cores:       {sys_info['cores']}")
    print(f"  Memory:      {sys_info['memory_gb']} GB")
    print(f"  Cycles:      {BENCH_CYCLES} per run ({WARMUP_CYCLES} warmup, "
          f"{MEASURED_PER_RUN} measured)")
    print(f"  Runs:        {RUNS} (total {TOTAL_MEASURED} measured cycles)")
    print()

    # Ensure binaries
    print(f"{BLUE}[Step 1] Checking binaries...{NC}")
    old_binary = ensure_baseline()
    new_binary = ensure_current()
    print()

    # Get HEAD commit SHA
    head_sha = subprocess.getoutput(
        f"git -C {REPO_ROOT} rev-parse --short HEAD"
    )

    # Run benchmarks
    print(f"{BLUE}[Step 2] Running benchmarks ({RUNS} runs x {BENCH_CYCLES} "
          f"cycles each)...{NC}")
    print()

    old = run_suite(old_binary, "v1.4.6 (upstream baseline)")
    new = run_suite(new_binary, f"Current HEAD ({head_sha}, post-v1.2)")

    # Save raw data
    with open(os.path.join(RESULTS_DIR, "v12_old_bench.json"), "w") as f:
        json.dump({"label": "v1.4.6", "binary": old_binary, **old}, f,
                  indent=2, default=str)
    with open(os.path.join(RESULTS_DIR, "v12_new_bench.json"), "w") as f:
        json.dump({"label": f"HEAD ({head_sha})", "binary": new_binary, **new},
                  f, indent=2, default=str)

    # Print comparison
    print()
    changes = print_comparison(old, new, old_binary, new_binary)

    # Save comparison report
    report = {
        "system": sys_info,
        "config": {
            "bench_cycles": BENCH_CYCLES,
            "warmup_cycles": WARMUP_CYCLES,
            "runs": RUNS,
            "measured_per_run": MEASURED_PER_RUN,
            "measured_cycles_total": TOTAL_MEASURED,
            "cmake_flags": " ".join(CMAKE_FLAGS),
            "baseline_version": "v1.4.6",
            "current_commit": head_sha,
        },
        "old": old,
        "new": new,
        "changes": changes,
        "binaries": {
            "old": {"path": old_binary, "size_kb": changes["old_binary_kb"]},
            "new": {"path": new_binary, "size_kb": changes["new_binary_kb"]},
        },
    }
    report_path = os.path.join(RESULTS_DIR, "v12_comparison_report.json")
    with open(report_path, "w") as f:
        json.dump(report, f, indent=2, default=str)

    print()
    print(f"  {GREEN}Report saved to {report_path}{NC}")

    return report


if __name__ == "__main__":
    main()
