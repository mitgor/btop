---
phase: 02-string-allocation-reduction
verified: 2026-02-27T08:58:26Z
status: human_needed
score: 6/7 must-haves verified
re_verification: false
human_verification:
  - test: "Run heaptrack against 'btop --benchmark 100' with and without the regex, compare allocation trace"
    expected: "Heaptrack shows no std::regex allocation spike in the draw path; per-frame heap allocation count is measurably lower than baseline"
    why_human: "heaptrack is a runtime profiling tool that cannot be exercised in a static code check. Requires a running btop process and pre/post baseline recordings."
  - test: "Benchmark Fx::uncolor() against a regex-based reference implementation on a colored input string"
    expected: "Hand-written parser runs >= 10x faster than std::regex_replace on the same input (established by literature; regex std allocs on every call)"
    why_human: "The regex implementation was deleted so no A/B benchmark exists in bench_string_utils.cpp. The 10x claim relies on well-known std::regex overhead characteristics but cannot be measured programmatically from the current codebase."
  - test: "Run btop with and without the reserve multiplier changes on a 200x50 terminal, compare reallocation count via heaptrack"
    expected: "Heaptrack shows 0-2 reallocations per draw function call instead of 15-30 (plan estimate)"
    why_human: "Reserve sizing correctness requires runtime observation of actual string growth behavior; cannot be verified statically."
---

# Phase 2: String & Allocation Reduction Verification Report

**Phase Goal:** Per-frame heap allocation count in the draw path and string utilities is measurably reduced, with regex hot path eliminated
**Verified:** 2026-02-27T08:58:26Z
**Status:** human_needed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

All truths are drawn from the `must_haves.truths` blocks in both plan frontmatters, cross-referenced against ROADMAP.md success criteria.

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `Fx::uncolor()` produces identical output to the regex version for all SGR forms btop generates | VERIFIED | 14 GoogleTest cases (UncolorTest.*) all pass: reset, bold, 256-color FG/BG, truecolor FG/BG, consecutive, partial/malformed -- `ctest` shows 100% pass (15/15 including prior tools.string_split) |
| 2 | `Fx::uncolor()` runs 10x+ faster than the regex version on representative input strings | HUMAN NEEDED | No regex baseline remains in bench_string_utils.cpp to compare against; only new implementation is benchmarked. See human verification #2. |
| 3 | `uresize`, `luresize`, `ljust`, `rjust`, `cjust` accept `const string&` -- no string copy on function entry | VERIFIED | Header: all 5 signatures show `const string&` (btop_tools.hpp lines 171, 174, 286, 289, 292). Implementations: btop_tools.cpp lines 265, 296, 338, 353, 368 all use `const string& str`. In-place `str.resize()` replaced with `str.substr(0, i)` returns -- const-ref correctness confirmed. |
| 4 | All existing tests and benchmarks pass with the new implementations (plan 01) | VERIFIED | `ctest --test-dir build --output-on-failure` shows 15/15 tests passed; both `btop_bench_strings` and `btop_bench_draw` build and run without errors. |
| 5 | No `fmt::format()` calls remain in draw function hot paths -- all converted to `fmt::format_to(std::back_inserter(out), ...)` or eliminated | VERIFIED | `grep -c "fmt::format(" src/btop_draw.cpp` returns 2. Both are confirmed non-hot-path: line 200 (TextEdit, not per-frame render) and line 755 (battery watts, conditional display). 20 `fmt::format_to` calls confirmed. |
| 6 | Draw functions reserve output buffer sizes with escape-aware multipliers (>= 10x over raw width*height) | VERIFIED | 5 `out.reserve` calls found with multipliers: Cpu=16x (line 588), Gpu=16x (line 1045), Mem=12x (line 1223), Net=10x (line 1484), Proc=18x (line 1707). No bare `out.reserve(width * height)` without multiplier remains in any draw function. All have inline comments documenting the rationale. |
| 7 | All existing tests and benchmarks pass with the new draw code (plan 02) | VERIFIED | `cmake --build build --target btop` succeeds. `cmake --build build-bench --target btop_bench_draw` succeeds. `cmake --build build-bench --target btop_bench_strings` succeeds. Full `ctest` passes 15/15. |

**Score:** 6/7 truths verified (1 human-needed)

---

## Required Artifacts

### Plan 02-01 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/btop_tools.hpp` | Hand-written `uncolor()` declaration (non-inline), `const string&` signatures for 5 utility functions | VERIFIED | Line 89: `string uncolor(const string& s);` (declaration only, not inline). Lines 171/174/286/289/292: all 5 functions show `const string&`. No `color_regex`, `escape_regex`, or `#include <regex>` present. |
| `src/btop_tools.cpp` | Single-pass forward-copy `Fx::uncolor()` implementation; `const string&` implementations for all 5 functions | VERIFIED | Lines 187-209: `namespace Fx { string uncolor(const string& s) {...} }` -- forward-copy O(n) loop, `out.reserve(s.size())`, skips ESC[...m sequences. Lines 265/296/338/353/368: all 5 functions confirmed `const string&`. |
| `tests/test_uncolor.cpp` | GoogleTest suite with `TEST(UncolorTest` cases | VERIFIED | File exists. 14 test cases covering EmptyString, NoEscapes, OnlyEscapes, ResetSequence, BoldSequence, Color256Foreground/Background, TruecolorForeground/Background, ConsecutiveEscapes, MixedTextAndEscapes, NonSGRSequencePreserved, PartialEscapeAtEnd, LargeString. All 14 pass. |
| `tests/CMakeLists.txt` | Test target includes `test_uncolor.cpp` | VERIFIED | Line 16: `add_executable(btop_test tools.cpp test_uncolor.cpp)` |

### Plan 02-02 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/btop_draw.cpp` | Draw functions using `fmt::format_to` and escape-aware `out.reserve` estimates | VERIFIED | 20 `fmt::format_to(std::back_inserter(...)` calls present. 5 `out.reserve` calls with multipliers 10-18x. Only 2 `fmt::format(` calls remain (both confirmed non-hot-path). |

---

## Key Link Verification

### Plan 02-01 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/btop_tools.hpp` | `src/btop_tools.cpp` | `Fx::uncolor` declaration in header, implementation in cpp | WIRED | Header line 89: `string uncolor(const string& s);` (declaration only). Cpp lines 187-209: `namespace Fx { string uncolor(const string& s) {...} }`. The old inline regex implementation is gone; the forward-copy parser is live in the cpp. |
| `tests/test_uncolor.cpp` | `src/btop_tools.hpp` | `#include "btop_tools.hpp"` + `Fx::uncolor(...)` calls | WIRED | Line 21: `#include "btop_tools.hpp"`. Lines 26, 30, 34, 38, 42, 46, 50, 54, 58, 62, 67, 76, 84-86, 96: `Fx::uncolor(...)` called in every test case. |

### Plan 02-02 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/btop_draw.cpp` | `include/fmt/base.h` | `fmt::format_to(std::back_inserter(out), ...)` pattern | WIRED | 20 occurrences of `fmt::format_to(std::back_inserter(` found. Line 29: `#include <fmt/format.h>` provides the format_to template. Pattern used in Cpu::draw, Gpu::draw, Net::draw, Proc::draw, createBox. |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| STRN-01 | 02-01-PLAN.md | `std::regex` in `Fx::uncolor()` replaced with hand-written ANSI escape code parser | SATISFIED | `btop_tools.hpp` has no `color_regex`, no `escape_regex`, no `#include <regex>`. `btop_tools.cpp` has forward-copy O(n) parser at lines 187-209. 14 correctness tests pass. `btop_tools.hpp` previously had `inline string uncolor(...) { return std::regex_replace(...); }` -- fully replaced. |
| STRN-02 | 02-01-PLAN.md | String-by-value parameters in utility functions replaced with `const string&` | SATISFIED | All 5 functions (uresize, luresize, ljust, rjust, cjust) take `const string&` in both declaration (btop_tools.hpp) and implementation (btop_tools.cpp). In-place mutations replaced with `substr` returns. 127+ call sites unchanged (source-compatible). |
| STRN-03 | 02-02-PLAN.md | `fmt::format` calls in draw pipeline replaced with `fmt::format_to` to eliminate intermediate string allocations | SATISFIED | 2 `fmt::format(` calls remain (both non-hot-path). 20 `fmt::format_to(std::back_inserter(` calls present. 11 conversions made per summary (lines 291, 297, 855, 935, 982, 1112, 1979, 2003, 2116, 2120, 2132). |
| STRN-04 | 02-02-PLAN.md | `string::reserve()` calls added to draw functions with accurate size estimates accounting for escape code overhead | SATISFIED | 5 reserve calls with multipliers: Cpu 16x, Gpu 16x, Mem 12x, Net 10x, Proc 18x. All have inline comments. No bare `width * height` reserve without multiplier remains in any draw function. |

**Orphaned requirements check:** REQUIREMENTS.md maps exactly STRN-01, STRN-02, STRN-03, STRN-04 to Phase 2. All 4 are claimed by plan frontmatter. Zero orphaned requirements.

---

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/btop_draw.cpp` | 895 | `// FIXME: This should be checked during collection...` | Info | Pre-existing comment; not introduced by phase 02 commits (verified via `git show eff1bed` diff). No impact on phase 02 goal. |
| `src/btop_draw.cpp` | 1055 | `// TODO cpu -> gpu` | Info | Pre-existing comment; not introduced by phase 02 commits. Labels a graph symbol string, not blocking. |
| `src/btop_draw.cpp` | 2396 | `// TODO gpu_box` | Info | Pre-existing comment; not introduced by phase 02 commits. Labeling code, not blocking. |

All three are pre-existing annotations not touched by phase 02. None are stubs or placeholders in phase 02 work.

---

## Human Verification Required

### 1. heaptrack: Regex Allocation Spike Elimination

**Test:** Run `heaptrack btop --benchmark 100` on a commit *before* b2d6170 (the regex removal), then on the current HEAD. Compare allocation traces using `heaptrack_print`. Filter for `std::regex` allocations in the draw/uncolor path.
**Expected:** Before: visible per-call regex allocations in uncolor call stack. After: no regex allocations in uncolor call stack; heap usage from that path is eliminated.
**Why human:** heaptrack requires a running process, two separate builds, and manual comparison of allocation traces. Cannot be exercised in a static code check.

### 2. 10x+ Performance Improvement Confirmation for Uncolor

**Test:** Add a regex baseline benchmark to `benchmarks/bench_string_utils.cpp`, build with `DBTOP_BENCHMARKS=ON`, run `./build-bench/benchmarks/btop_bench_strings`, compare `Fx::uncolor` vs `std::regex_replace` throughput on the same colored_line input.
**Expected:** Hand-written parser is >= 10x faster (147 ns/op measured for current; std::regex typically 1500-5000 ns/op for this input size based on known std::regex overhead).
**Why human:** The regex implementation was deleted from the codebase as intended. No A/B comparison exists in the current benchmark suite. Requires either adding a temporary benchmark or accepting the well-documented std::regex overhead characteristics as sufficient justification.

### 3. heaptrack: Reserve Multiplier Reallocation Reduction

**Test:** Run `heaptrack btop --benchmark 100` on current HEAD on a 200x50 terminal. Examine heap traces for `string::reserve` / `string::_M_mutate` / `__gxx_personality_v0` calls originating from Cpu::draw, Mem::draw, Net::draw, Proc::draw. Count reallocations per frame.
**Expected:** 0-2 reallocations per draw function call instead of the pre-multiplier estimate of 15-30 per function.
**Why human:** Runtime profiling required; string growth behavior is data-dependent and varies with actual terminal size and btop content.

---

## Gaps Summary

No blocking gaps were found. All structural changes are fully implemented and verified:

- `Fx::uncolor()` is a single-pass O(n) forward-copy parser with no std::regex dependency.
- `color_regex`, `escape_regex`, and the `<regex>` include are gone from `btop_tools.hpp`.
- All 5 string utility functions take `const string&`.
- 11 of 13 `fmt::format()` calls converted to `fmt::format_to()` (2 non-hot-path retained by design).
- 5 draw function `out.reserve()` calls updated with escape-aware multipliers (10x-18x).
- 14/14 uncolor correctness tests pass.
- btop binary builds and links successfully.
- All 4 requirements (STRN-01 through STRN-04) are satisfied.

The 3 human verification items are confirmatory measurements (heaptrack, baseline benchmark) that the plan identified as manual-only in the research document. They do not indicate missing implementation -- they confirm the magnitude of the improvement that has already been made.

---

_Verified: 2026-02-27T08:58:26Z_
_Verifier: Claude (gsd-verifier)_
