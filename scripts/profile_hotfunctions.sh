#!/usr/bin/env bash
set -euo pipefail

# profile_hotfunctions.sh -- macOS CPU sampling profiler for btop
#
# Launches btop in benchmark mode, attaches the macOS native `sample`
# profiler, parses the output, and prints a ranked hot-function table
# with CPU % attribution.
#
# Usage: bash scripts/profile_hotfunctions.sh [benchmark_cycles]
#   benchmark_cycles  Number of collect+draw cycles (default: 200)
#
# Requirements:
#   - macOS with /usr/bin/sample available
#   - build-bench/btop binary built with -DBTOP_BENCHMARKS=ON
#
# Output:
#   - Markdown table on stdout (ranked functions by CPU %)
#   - Raw sample output saved to /tmp/btop_profile.txt

BENCHMARK_CYCLES="${1:-200}"
SAMPLE_DURATION=5
RAW_OUTPUT="/tmp/btop_profile.txt"
BTOP_BINARY="./build-bench/btop"

# ── Prerequisites ──────────────────────────────────────────────────
if [[ ! -x "$BTOP_BINARY" ]]; then
    echo "ERROR: $BTOP_BINARY not found or not executable." >&2
    echo "Build with: cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DBTOP_BENCHMARKS=ON && cmake --build build-bench -j\$(sysctl -n hw.logicalcpu)" >&2
    exit 1
fi

if ! command -v sample &>/dev/null; then
    echo "ERROR: macOS 'sample' command not found. This script requires macOS." >&2
    exit 1
fi

# ── Launch btop in benchmark mode ──────────────────────────────────
echo "Launching btop --benchmark ${BENCHMARK_CYCLES} ..." >&2
"$BTOP_BINARY" --benchmark "$BENCHMARK_CYCLES" > /tmp/btop_benchmark_output.json 2>/dev/null &
BTOP_PID=$!

# Wait for btop to start doing work
sleep 0.5

# Verify btop is still running
if ! kill -0 "$BTOP_PID" 2>/dev/null; then
    echo "WARNING: btop exited before sampling could start. Retrying with more cycles..." >&2
    BENCHMARK_CYCLES=500
    "$BTOP_BINARY" --benchmark "$BENCHMARK_CYCLES" > /tmp/btop_benchmark_output.json 2>/dev/null &
    BTOP_PID=$!
    sleep 0.5
    if ! kill -0 "$BTOP_PID" 2>/dev/null; then
        echo "ERROR: btop exits too quickly for sampling. Cannot profile." >&2
        exit 1
    fi
fi

echo "btop PID: ${BTOP_PID}" >&2
echo "Sampling for ${SAMPLE_DURATION} seconds ..." >&2

# ── Attach macOS sample profiler ───────────────────────────────────
# The sample command captures CPU call stacks at ~1ms intervals
sample "$BTOP_PID" "$SAMPLE_DURATION" -f "$RAW_OUTPUT" 2>/dev/null || {
    # If sample fails (process exited during sampling), check if we got partial data
    if [[ -s "$RAW_OUTPUT" ]]; then
        echo "WARNING: sample exited early but captured partial data." >&2
    else
        echo "ERROR: sample failed and produced no output." >&2
        # Clean up btop if still running
        kill "$BTOP_PID" 2>/dev/null || true
        wait "$BTOP_PID" 2>/dev/null || true
        exit 1
    fi
}

# Wait for btop to finish (it may still be running)
kill "$BTOP_PID" 2>/dev/null || true
wait "$BTOP_PID" 2>/dev/null || true

echo "Raw profile saved to: ${RAW_OUTPUT}" >&2
echo "" >&2

# ── Parse the sample output ───────────────────────────────────────
# The macOS sample tool outputs a "Sort by top of stack" section near
# the end. Each line in that section looks like:
#     function_name    <count>
# We parse those lines, compute total samples, and derive percentages.

if [[ ! -s "$RAW_OUTPUT" ]]; then
    echo "ERROR: Raw profile output is empty." >&2
    exit 1
fi

# Extract the "Sort by top of stack" section
# This section starts with a line containing "Sort by top of stack"
# and continues until the next section or end of file
awk '
BEGIN { in_section = 0; total = 0; idx = 0 }
/Sort by top of stack/ { in_section = 1; next }
/^$/ && in_section { next }
/Sort by / && in_section && !/Sort by top of stack/ { in_section = 0; next }
in_section && /[0-9]+/ {
    # Lines look like:  function_name    count
    # Trim leading/trailing whitespace, extract last field as count
    line = $0
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
    if (line == "") next

    # The count is the last whitespace-separated field
    count = $NF
    if (count+0 != count) next  # skip if last field is not numeric

    # Function name is everything before the last field
    fname = ""
    for (i = 1; i < NF; i++) {
        if (i > 1) fname = fname " "
        fname = fname $i
    }
    if (fname == "") next

    names[idx] = fname
    counts[idx] = count + 0
    total += count + 0
    idx++
}
END {
    if (total == 0) {
        print "ERROR: No samples found in profile output." > "/dev/stderr"
        exit 1
    }

    # Sort by count descending (simple bubble sort for awk)
    for (i = 0; i < idx - 1; i++) {
        for (j = i + 1; j < idx; j++) {
            if (counts[j] > counts[i]) {
                tmp = counts[i]; counts[i] = counts[j]; counts[j] = tmp
                tmp = names[i]; names[i] = names[j]; names[j] = tmp
            }
        }
    }

    # Print markdown table header
    print "| Rank | Function | Samples | CPU % |"
    print "|------|----------|---------|-------|"

    # Print top functions (up to 30 or all if fewer)
    limit = (idx < 30) ? idx : 30
    for (i = 0; i < limit; i++) {
        pct = (counts[i] / total) * 100
        printf "| %d | %s | %d | %.1f%% |\n", i+1, names[i], counts[i], pct
    }

    # Print total
    printf "\n**Total samples: %d**\n", total
}
' "$RAW_OUTPUT"
