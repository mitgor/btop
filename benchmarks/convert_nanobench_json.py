#!/usr/bin/env python3
"""Convert nanobench and btop benchmark JSON to github-action-benchmark format.

Reads one or more JSON files (nanobench output or btop --benchmark output)
and produces a combined customSmallerIsBetter JSON array on stdout.

Usage:
    python3 convert_nanobench_json.py bench_strings.json bench_macro.json
"""

import json
import sys


def convert_nanobench(data):
    """Convert nanobench JSON to customSmallerIsBetter entries.

    nanobench JSON format has each benchmark as a top-level entry in the
    "results" array (not nested inside a "benchmarks" sub-array).
    Each entry has "name" and "median(elapsed)" fields directly.
    """
    entries = []
    for result in data.get("results", []):
        # Each result is a single benchmark entry with name and median(elapsed)
        name = result.get("name", "unknown")
        median_elapsed = result.get("median(elapsed)", 0)
        entries.append({
            "name": name,
            "unit": "ns/op",
            "value": round(median_elapsed * 1e9, 2)
        })
    return entries


def convert_btop_benchmark(data):
    """Convert btop --benchmark JSON to customSmallerIsBetter entries."""
    entries = []
    summary = data.get("benchmark", {}).get("summary", {})
    mapping = {
        "mean_wall_us": "btop_cycle_wall",
        "mean_collect_us": "btop_cycle_collect",
        "mean_draw_us": "btop_cycle_draw",
    }
    for key, short_name in mapping.items():
        if key in summary:
            entries.append({
                "name": short_name,
                "unit": "us/cycle",
                "value": round(summary[key], 2)
            })
    return entries


def main():
    if len(sys.argv) < 2:
        print(
            "Usage: convert_nanobench_json.py <file1.json> [file2.json ...]",
            file=sys.stderr,
        )
        sys.exit(1)

    all_entries = []
    for path in sys.argv[1:]:
        try:
            with open(path) as f:
                data = json.load(f)
        except (json.JSONDecodeError, FileNotFoundError) as e:
            print(f"Warning: Failed to read {path}: {e}", file=sys.stderr)
            continue

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

    json.dump(all_entries, sys.stdout, indent=2)
    print()  # trailing newline


if __name__ == "__main__":
    main()
