# Phase 2: String & Allocation Reduction - Research

**Researched:** 2026-02-27
**Domain:** C++ string optimization, ANSI escape code parsing, fmt library usage, heap allocation reduction
**Confidence:** HIGH

## Summary

Phase 2 targets four concrete allocation hotspots in btop's per-frame render path: (1) `Fx::uncolor()` using `std::regex_replace` on every frame when overlays are active, (2) string utility functions (`uresize`, `luresize`, `ljust`, `rjust`, `cjust`) that accept `string` parameters by value, forcing unnecessary copies at every call site, (3) `fmt::format` calls that allocate intermediate `std::string` objects instead of appending directly to the output buffer, and (4) draw functions that use `out.reserve(width * height)` as a rough size estimate without accounting for ANSI escape code overhead, causing repeated reallocations.

The codebase is C++23 (CMakeLists.txt line 25), uses fmt 12.0.0 in header-only mode, and already has a `libbtop` OBJECT library that benchmarks and tests link against. Phase 1 established nanobench micro-benchmarks in `benchmarks/bench_string_utils.cpp` and `benchmarks/bench_draw.cpp` that specifically cover `Fx::uncolor`, `uresize`, `ljust`, `rjust`, `cjust`, and draw composition patterns -- these provide direct before/after measurement for every change in this phase.

The most impactful change is replacing `std::regex_replace` in `Fx::uncolor()`. The codebase already contains a commented-out hand-written parser (btop_tools.cpp lines 187-210) that was reverted in v1.2.2 due to a musl-related delay in the menu. The old parser had a bug: it used `out.erase()` in a loop (O(n^2) due to repeated string shuffling) and had incorrect escape detection logic. A correct implementation should scan forward, copy non-escape characters, and skip escape sequences -- a single-pass O(n) algorithm with no intermediate allocations. The `color_regex` pattern matches `\033[\d+;?\d?;?\d*;?\d*;?\d*m` which corresponds to SGR (Select Graphic Rendition) sequences. btop generates exactly three forms: simple (`\033[Nm`), 256-color (`\033[38;5;Nm` / `\033[48;5;Nm`), and truecolor (`\033[38;2;R;G;Bm` / `\033[48;2;R;G;Bm`). A state machine that recognizes `ESC [ digits-and-semicolons m` covers all cases.

**Primary recommendation:** Implement all four requirements in order of impact: uncolor parser first (highest allocation spike), then parameter-passing fixes (zero-risk refactor), then fmt::format_to conversion (moderate allocation reduction), then reserve estimates (polish).

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| STRN-01 | std::regex in Fx::uncolor() replaced with hand-written ANSI escape code parser (10-100x improvement target) | Confirmed: `uncolor()` at btop_tools.hpp:96 uses `std::regex_replace` with `color_regex`. Prior hand-written version existed but was buggy (O(n^2) erase pattern, incorrect digit check). A single-pass copy-and-skip parser avoids both regex allocation and the old erase-in-loop bug. Existing bench at bench_string_utils.cpp:47 measures this directly. The `escape_regex` constant is defined but never used anywhere and can be removed alongside. |
| STRN-02 | String-by-value parameters in utility functions replaced with const ref or string_view | Confirmed: `uresize(const string str, ...)` at btop_tools.hpp:179 takes by value (copy on call). Same for `luresize` (line 182), `ljust` (line 294), `rjust` (line 297), `cjust` (line 300). These are called 127 times across the codebase (90 in btop_draw.cpp alone, 23 in btop_menu.cpp). Changing to `const string&` eliminates 127+ copies per frame. Benches at bench_string_utils.cpp:57-75 measure these. |
| STRN-03 | fmt::format calls in draw pipeline replaced with fmt::format_to to eliminate intermediate string allocations | Confirmed: btop_draw.cpp has 13 `fmt::format()` calls and 11 `fmt::format_to()` calls. The 13 `fmt::format()` calls each allocate+return an intermediate `std::string`. The codebase already uses `fmt::format_to(std::back_inserter(out), ...)` in several places (e.g., lines 612, 1830, 1972, 2047, 2144, 2193), proving the pattern works with fmt 12.0.0 and named args. Converting the remaining 13 calls eliminates 13 intermediate string allocations per frame. |
| STRN-04 | String reserve() calls added to draw functions with accurate size estimates accounting for escape code overhead | Confirmed: 5 draw functions use `out.reserve(width * height)` (lines 588, 1042, 1218, 1479, 1702). A typical btop cell contains 1 visible character + ~12-20 bytes of escape codes (e.g., `\033[38;2;R;G;Bm` = 19 bytes). Current estimate of `width * height` bytes is ~10-20x too low. An estimate of `width * height * 20` (or a measured constant) would prevent repeated reallocations. The `out +=` pattern is used 162 times in btop_draw.cpp, with 111 of those being multi-concatenation chains (`out += a + b + c`), each potentially triggering growth. |
</phase_requirements>

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| **fmt** | 12.0.0 (header-only, vendored in `include/fmt/`) | `fmt::format_to` with `std::back_inserter` for zero-alloc string composition | Already used throughout the codebase. `format_to` is the canonical way to avoid intermediate string allocations. Named args (`"key"_a = val`) already used in btop_draw.cpp. **Confidence: HIGH** |
| **nanobench** | 4.3.11 (FetchContent in `benchmarks/CMakeLists.txt`) | Before/after micro-benchmarks for every change | Already set up in Phase 1 with specific benchmarks for uncolor, uresize, ljust, rjust, cjust, and draw composition. **Confidence: HIGH** |
| **heaptrack** | 1.5+ | Heap allocation counting to verify allocation reduction | Recommended in Phase 1 research for measuring per-cycle allocation counts. Run on `btop --benchmark N` mode. **Confidence: HIGH** |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **GoogleTest** | 1.17.0 (FetchContent in `tests/CMakeLists.txt`) | Correctness tests for uncolor parser | Use to verify the hand-written parser produces identical output to the regex version for all escape code forms btop generates |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Hand-written ANSI parser | RE2 (Google's regex library) | RE2 avoids std::regex's worst-case exponential behavior but still allocates. A hand-written parser for this specific, well-defined grammar is simpler and zero-alloc. |
| `const string&` params | `std::string_view` params | string_view avoids copies AND avoids ref-counting. However, ljust/rjust/cjust internally modify the string (resize, append spaces), so they need an owned copy eventually. `const string&` is the safe, minimal change. uresize/luresize could potentially use string_view for the input but must return string. |
| `fmt::format_to` | Direct `out.append()` chains | `append()` avoids format string parsing overhead but sacrifices readability. `fmt::format_to` is the right balance for cases already using `fmt::format`. For simple `out += a + b + c` chains, converting to `out += a; out += b; out += c;` or `out.append(a).append(b)` may be more appropriate than introducing fmt where it wasn't used before. |

## Architecture Patterns

### Pattern 1: Single-Pass ANSI Escape Stripper

**What:** A hand-written parser that scans the input string once, copies non-escape characters to output, and skips `ESC [ <params> m` sequences entirely.

**When to use:** Replacing `Fx::uncolor()` (STRN-01).

**Key insight:** btop only generates SGR sequences (ending in `m`). The `color_regex` pattern is:
```
\033\[\d+;?\d?;?\d*;?\d*;?\d*m
```
This matches sequences like:
- `\033[0m` (reset)
- `\033[1m` (bold)
- `\033[38;5;196m` (256-color)
- `\033[38;2;255;128;0m` (truecolor)
- `\033[48;2;255;128;0m` (truecolor background)

**Implementation approach:**
```cpp
string uncolor(const string& s) {
    string out;
    out.reserve(s.size());  // worst case: no escapes
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            // Skip ESC[
            i += 2;
            // Skip digits and semicolons
            while (i < s.size() && (isdigit(s[i]) || s[i] == ';')) {
                i++;
            }
            // Skip the terminating 'm' if present
            if (i < s.size() && s[i] == 'm') {
                i++;
            }
            // If not 'm', this wasn't a color sequence -- the regex
            // also wouldn't match it, so we correctly skip nothing
            // (the ESC[ prefix is already consumed, matching regex behavior)
        } else {
            out += s[i++];
        }
    }
    return out;
}
```

**Critical correctness notes:**
1. The old commented-out parser (btop_tools.cpp:187-210) used `out.erase()` in a loop, which is O(n^2) for strings with many escape codes. The new approach copies forward, which is O(n).
2. The old parser checked `isdigit(out[end_pos - 1])` which is insufficient -- it missed sequences like `\033[0m` where '0' is a digit but the parser could still miss valid sequences depending on its `find('m')` logic.
3. The musl issue (CHANGELOG: v1.2.2) was reportedly a "delay in opening menu" -- this was likely caused by the O(n^2) erase pattern on large strings, not by the parsing logic itself. A forward-copy parser eliminates this entirely.
4. The regex `color_regex` only matches sequences ending in `m`. The `escape_regex` also matches `f`, `s`, `u`, `C`, `D`, `A`, `B` terminators (cursor movement, save/restore) but is never used anywhere in the codebase. The hand-written parser should match `color_regex` behavior (only strip `m`-terminated sequences).

### Pattern 2: Parameter Passing Optimization

**What:** Change function signatures from pass-by-value to pass-by-const-reference.

**When to use:** STRN-02 -- `uresize`, `luresize`, `ljust`, `rjust`, `cjust`.

**Current signatures (btop_tools.hpp):**
```cpp
string uresize(const string str, const size_t len, bool wide = false);       // line 179
string luresize(const string str, const size_t len, bool wide = false);      // line 182
string ljust(string str, const size_t x, bool utf = false, ...);             // line 294
string rjust(string str, const size_t x, bool utf = false, ...);             // line 297
string cjust(string str, const size_t x, bool utf = false, ...);             // line 300
```

**Target signatures:**
```cpp
string uresize(const string& str, const size_t len, bool wide = false);
string luresize(const string& str, const size_t len, bool wide = false);
string ljust(const string& str, const size_t x, bool utf = false, ...);
string rjust(const string& str, const size_t x, bool utf = false, ...);
string cjust(const string& str, const size_t x, bool utf = false, ...);
```

**Implementation notes:**
- `uresize` currently modifies `str` in the non-wide path (line 266-297 of btop_tools.cpp): it calls `str.resize(i)` and `str.shrink_to_fit()`. With const ref, this must create a local copy only when needed (i.e., when the string actually needs truncation). The wide path already creates `n_str` separately.
- `luresize` uses `str.substr()` and `str = str.substr(last_pos)` -- needs a local copy.
- `ljust`/`rjust`/`cjust` call `str.resize(x)` in some branches -- need local copies in those branches.
- The key optimization: in the common case where the string doesn't need modification (already fits), no copy is made at all. Currently a copy is always made on function entry.

### Pattern 3: fmt::format_to Conversion

**What:** Replace `fmt::format(...)` with `fmt::format_to(std::back_inserter(out), ...)` in draw functions.

**When to use:** STRN-03 -- all 13 remaining `fmt::format()` calls in btop_draw.cpp.

**Before:**
```cpp
out += fmt::format("{}{}{}{}{}{}{}{}{}", a, b, c, d, e, f, g, h, i);
```

**After:**
```cpp
fmt::format_to(std::back_inserter(out), "{}{}{}{}{}{}{}{}{}", a, b, c, d, e, f, g, h, i);
```

**Existing examples in codebase** (already converted):
- Line 612: `fmt::format_to(std::back_inserter(out), "{}{}{}{}{}", ...)`
- Line 1972: `fmt::format_to(std::back_inserter(out), "{move}{unbold}{graph}...", "move"_a = ...)`
- Line 2144: `fmt::format_to(std::back_inserter(out), "{}{}{}{}{:^{}}{}", ...)`

**Remaining calls to convert** (with line numbers):
- Line 200 (TextEdit) -- low priority, not in render hot path
- Line 291, 297 (createBox) -- called during redraw only
- Line 755, 854, 934, 981, 1109 (Cpu::draw) -- in per-frame render
- Line 1975 (Proc::draw detailed) -- nested fmt::format inside format_to
- Line 1993, 2105, 2108, 2116 (Proc::draw per-process loop) -- **highest priority**, called once per visible process per frame

### Pattern 4: Reserve with Escape-Aware Size Estimates

**What:** Replace `out.reserve(width * height)` with estimates that account for ANSI escape code overhead.

**When to use:** STRN-04 -- all 5 draw functions.

**Current reserves:**
```cpp
out.reserve(width * height);  // ~4,000-10,000 bytes for typical terminal
```

**Reality:** A typical btop output line contains:
- `Mv::to(y, x)` = `\033[Y;Xf` = ~8-12 bytes per cursor position
- Color codes = `\033[38;2;R;G;Bm` = ~19 bytes per color change
- Multiple color changes per cell (foreground, background, style)
- A process list row might be: cursor(12) + 5 colors(95) + text(80) = ~187 bytes for 80 visible characters

**Recommended estimate:** `width * height * 20` as a conservative multiplier. This accounts for ~20 bytes of escape overhead per visible character position. For a 200x50 terminal, this gives 200,000 bytes (200KB) per frame, which is reasonable for a single string allocation.

**Alternative: Measure actual output size** during the first few frames and use that as the estimate (adaptive reserve). This is more accurate but adds complexity.

### Anti-Patterns to Avoid

- **Multi-concatenation chains as temporaries:** `out += a + b + c + d` creates up to 3 temporary strings. Use `out += a; out += b; out += c; out += d;` or `fmt::format_to(std::back_inserter(out), "{}{}{}{}", a, b, c, d)` instead. There are 111 such chains in btop_draw.cpp. However, converting ALL 111 in this phase would be a massive diff with high regression risk. Focus on the ones inside per-process loops and per-frame code paths. The `redraw`-only sections (executed once on resize) are lower priority.
- **Over-converting simple appends:** `out += Fx::reset` is a single append of a small string. Converting this to `fmt::format_to(std::back_inserter(out), "{}", Fx::reset)` adds format string parsing overhead for no benefit. Only convert cases with multiple arguments.
- **Breaking the musl build:** The hand-written uncolor parser must be tested against musl-compiled builds. The CI already has a static musl build (cmake-linux.yml). Ensure the new parser works correctly there.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ANSI escape code detection | Full VT100/xterm terminal emulator parser | Simple `ESC [ digits-and-semicolons m` scanner | btop only generates SGR sequences. A full parser (handling OSC, DCS, CSI with all terminators) is unnecessary complexity. |
| String formatting with argument insertion | Custom `snprintf`-based formatter | `fmt::format_to` | fmt is already vendored and used throughout. It handles buffer management, type safety, and named arguments. |
| Allocation profiling | Manual `malloc` hooks or `new` overrides | heaptrack | heaptrack provides call stacks, allocation counts, and timeline visualization without modifying the code. |

**Key insight:** The temptation is to convert ALL `out +=` chains to `fmt::format_to` for "consistency." Resist this -- many simple appends (`out += Mv::to(y, x)`) are already optimal. Focus format_to conversion on cases that currently use `fmt::format` (which allocates) and multi-arg concatenation chains in hot loops.

## Common Pitfalls

### Pitfall 1: O(n^2) String Erase Pattern
**What goes wrong:** The previous hand-written uncolor (btop_tools.cpp:187-210) used `out.erase(start_pos, len)` in a loop. Each erase shifts all subsequent characters, making the total cost O(n * k) where k is the number of escape sequences.
**Why it happens:** The intuitive approach is "find and remove" rather than "copy what you keep."
**How to avoid:** Use a forward-copy pattern: scan input, copy non-escape chars to output, skip escapes. Single pass, O(n).
**Warning signs:** `string::erase()` or `string::replace()` called inside a loop over the same string.

### Pitfall 2: Regression in uncolor Behavior
**What goes wrong:** The hand-written parser strips sequences the regex wouldn't (or vice versa), causing visual artifacts in overlays.
**Why it happens:** The regex has specific semantics: it requires at least one digit after `\033[`, and only matches `m` as the terminator.
**How to avoid:** Write comprehensive tests comparing hand-written output to regex output on all escape code forms btop generates. Include edge cases: empty string, no escapes, only escapes, consecutive escapes, partial/malformed escapes.
**Warning signs:** Menu overlay text appears garbled or has visible escape codes.

### Pitfall 3: ljust/rjust/cjust Internal Mutation
**What goes wrong:** After changing parameters to const ref, the function body still tries to modify the input string (e.g., `str.resize(x)`).
**Why it happens:** The current implementations use the pass-by-value parameter as a mutable working copy.
**How to avoid:** In branches that modify the string, create a local copy. In the common "just pad" path, construct the result directly without copying the input.
**Warning signs:** Compilation errors (`cannot modify const reference`) or unexpected test failures.

### Pitfall 4: fmt::format_to with Named Args Compilation
**What goes wrong:** Named arguments (`"key"_a = val`) with `format_to` may have different compile-time checking behavior than with `format`.
**Why it happens:** fmt's compile-time format string checking works differently for `format` vs `format_to`.
**How to avoid:** The codebase already uses `format_to` with named args successfully (line 1972). Follow the existing pattern exactly. Test compilation on all CI platforms (GCC 14, Clang 19-21).
**Warning signs:** Template errors mentioning `basic_format_string` or `format_arg`.

### Pitfall 5: Reserve Estimate Too Large
**What goes wrong:** Reserving `width * height * 20` for every draw function wastes memory for small terminals or simple layouts.
**Why it happens:** Overly conservative estimate applied uniformly.
**How to avoid:** Use per-function estimates: Proc::draw (most text) needs higher multiplier than Net::draw (mostly graphs). Alternatively, use `max(width * height * 10, 4096)` as a floor-and-multiplier approach.
**Warning signs:** heaptrack shows a single large allocation where there used to be many small ones (correct trade-off), but RSS increases noticeably on small terminals.

## Code Examples

### Example 1: Correct uncolor() Implementation

```cpp
// In btop_tools.hpp (replacing the inline regex version at line 96)
string uncolor(const string& s);

// In btop_tools.cpp
namespace Fx {
    string uncolor(const string& s) {
        string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ) {
            // Check for ESC [ sequence
            if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
                size_t j = i + 2;
                // Skip digits and semicolons (SGR parameters)
                while (j < s.size() && ((s[j] >= '0' && s[j] <= '9') || s[j] == ';')) {
                    j++;
                }
                // If terminated by 'm', this is an SGR sequence -- skip it
                if (j < s.size() && s[j] == 'm') {
                    i = j + 1;
                    continue;
                }
                // Not an SGR sequence -- copy the ESC literally
                // (matches regex behavior: regex wouldn't match this either)
            }
            out += s[i++];
        }
        return out;
    }
}
```

### Example 2: uresize with const ref

```cpp
// Declaration (btop_tools.hpp)
string uresize(const string& str, const size_t len, bool wide = false);

// Implementation (btop_tools.cpp)
string uresize(const string& str, const size_t len, bool wide) {
    if (len < 1 or str.empty())
        return "";

    if (wide) {
        // ... wide path unchanged (already creates separate n_str) ...
    }
    else {
        for (size_t x = 0, i = 0; i < str.size(); i++) {
            if ((static_cast<unsigned char>(str.at(i)) & 0xC0) != 0x80) x++;
            if (x >= len + 1) {
                return str.substr(0, i);  // Return substring, no in-place mutation
            }
        }
    }
    return str;  // String already fits, return copy (or original if RVO kicks in)
}
```

### Example 3: fmt::format to fmt::format_to Conversion

```cpp
// Before (btop_draw.cpp line 291-294):
out += fmt::format(
    "{}{}{}{}{}{}{}{}{}", Mv::to(y, x + 2), Symbols::title_left, Fx::b,
    numbering, Theme::c("title"), title, Fx::ub, line_color, Symbols::title_right
);

// After:
fmt::format_to(std::back_inserter(out),
    "{}{}{}{}{}{}{}{}{}", Mv::to(y, x + 2), Symbols::title_left, Fx::b,
    numbering, Theme::c("title"), title, Fx::ub, line_color, Symbols::title_right
);
```

### Example 4: Improved Reserve Estimate

```cpp
// Before:
out.reserve(width * height);

// After (in Proc::draw, which has the densest output):
// Each row: cursor_move(~12) + 5 color_changes(~95) + text(~width) + formatting(~20)
// Plus redraw box overhead (~width * 4)
out.reserve(width * height * 16);

// In Net::draw (sparse, mostly graphs):
out.reserve(width * height * 10);
```

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 + nanobench v4.3.11 |
| Config file | `tests/CMakeLists.txt` (GoogleTest), `benchmarks/CMakeLists.txt` (nanobench) |
| Quick run command | `cmake --build build-bench --target btop_bench_strings && ./build-bench/benchmarks/btop_bench_strings` |
| Full suite command | `cmake --build build -t btop_test && ctest --test-dir build && cmake --build build-bench --target btop_bench_strings btop_bench_draw && ./build-bench/benchmarks/btop_bench_strings && ./build-bench/benchmarks/btop_bench_draw` |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| STRN-01 | uncolor() produces identical output to regex version for all escape forms | unit | `ctest --test-dir build -R uncolor` | No -- Wave 0 |
| STRN-01 | uncolor() throughput improved 10x+ | benchmark | `./build-bench/benchmarks/btop_bench_strings --json` | Yes (bench_string_utils.cpp:47) |
| STRN-01 | heaptrack shows no regex allocation spike | manual | `heaptrack btop --benchmark 100` + `heaptrack_print` | Manual only |
| STRN-02 | No extra copies on call to uresize/ljust/etc | benchmark | `./build-bench/benchmarks/btop_bench_strings --json` | Yes (bench_string_utils.cpp:57-75) |
| STRN-03 | No intermediate string allocations from fmt::format in draw path | benchmark | `./build-bench/benchmarks/btop_bench_draw --json` | Yes (bench_draw.cpp:130) |
| STRN-04 | Reduced reallocation count per frame | manual | `heaptrack btop --benchmark 100` + count reallocs | Manual only |

### Sampling Rate

- **Per task commit:** `cmake --build build-bench --target btop_bench_strings && ./build-bench/benchmarks/btop_bench_strings`
- **Per wave merge:** Full test suite + all benchmarks
- **Phase gate:** Full suite green + heaptrack comparison before `/gsd:verify-work`

### Wave 0 Gaps

- [ ] `tests/test_uncolor.cpp` -- covers STRN-01 correctness (compare hand-written vs regex output)
- [ ] Add uncolor test target to `tests/CMakeLists.txt`
- [ ] Extend `bench_string_utils.cpp` with a "before/after" comparison variant for uresize const-ref

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| std::regex for string stripping | Hand-written parsers | Always for performance-critical paths | std::regex is notoriously slow in libstdc++ and libc++. 10-100x slower than hand-written for simple patterns. Known issue since C++11. |
| Pass-by-value for read-mostly params | const ref or string_view | C++11 (const ref), C++17 (string_view) | Eliminates unnecessary copies. Standard practice in modern C++. |
| fmt::format returning string | fmt::format_to appending to buffer | fmt 5.0+ (2018) | Eliminates intermediate allocation. fmt's own documentation recommends format_to for performance. |
| Ad-hoc reserve estimates | Measured/empirical reserves | General practice | Under-reserving causes O(n) reallocations during string growth. Over-reserving wastes memory. Profiling-driven estimates are best. |

**Deprecated/outdated:**
- std::regex: Not deprecated but widely considered unfit for performance-critical use in C++. No plans to fix the performance characteristics. The standard committee has acknowledged this.
- `escape_regex` constant in btop_tools.hpp: Defined but never used. Should be removed as dead code.

## Open Questions

1. **What was the exact musl issue in v1.2.2?**
   - What we know: The CHANGELOG says "delay in opening menu when compiled with musl." The commented-out code used `out.erase()` in a loop.
   - What's unclear: Whether the delay was purely due to the O(n^2) erase pattern or if there was a musl-specific string implementation issue.
   - Recommendation: The forward-copy parser avoids both possibilities. Test on musl via the existing CI static build.

2. **Should `escape_regex` be removed alongside the `color_regex` change?**
   - What we know: `escape_regex` is defined at btop_tools.hpp:90 but never referenced anywhere in the codebase.
   - What's unclear: Whether external plugins or forks depend on it.
   - Recommendation: Remove it. It's dead code in the Fx namespace. If truly needed externally, it can be restored.

3. **Should multi-concatenation chains (`out += a + b + c`) be addressed in this phase?**
   - What we know: 111 of 162 `out +=` calls in btop_draw.cpp use multi-concatenation. Each chain creates temporary strings.
   - What's unclear: How much allocation overhead they actually cause (SSO may handle many of the small intermediate strings).
   - Recommendation: Convert chains inside the per-process loop in Proc::draw (highest frequency). Defer the rest to a follow-up or Phase 5 (rendering optimization). Document this as a known improvement opportunity.

4. **Optimal reserve multiplier for each draw function?**
   - What we know: Current `width * height` is too low. A 200x50 terminal produces ~100-400KB of output per frame.
   - What's unclear: Exact per-function ratios without measurement.
   - Recommendation: Start with `width * height * 16` for all functions, then measure actual output size during Phase 1's `--benchmark` mode and adjust. Add a comment documenting the measured basis.

## Sources

### Primary (HIGH confidence)
- btop source code: btop_tools.hpp (lines 90-97, 179-182, 294-300), btop_tools.cpp (lines 187-210, 266-389), btop_draw.cpp (all draw functions)
- fmt 12.0.0 vendored headers: include/fmt/base.h (FMT_VERSION 120000, format_to API)
- CMakeLists.txt: C++23 standard (line 25), fmt header-only (line 153), benchmark infrastructure (lines 287-289)
- CHANGELOG.md: v1.2.0 (regex removed), v1.2.1 (uncolor bug), v1.2.2 (reverted to regex due to musl delay)

### Secondary (MEDIUM confidence)
- Phase 1 research and benchmarks: bench_string_utils.cpp, bench_draw.cpp provide baseline measurements
- Existing fmt::format_to usage in btop_draw.cpp (lines 612, 900, 907, 1830, 1887, 1941, 1972, 1977, 2047, 2144, 2193) validates the pattern works with this codebase

### Tertiary (LOW confidence)
- The exact performance ratio of hand-written vs std::regex (claimed 10-100x) is based on general C++ community benchmarks, not btop-specific measurement. Phase 1 benchmarks will provide the actual baseline. The improvement target should be validated against real measurements.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All tools already in use in the codebase (fmt, nanobench, GoogleTest)
- Architecture: HIGH - All four patterns are well-understood C++ optimization techniques with existing codebase precedent
- Pitfalls: HIGH - Historical musl issue is documented in CHANGELOG and commented code; parameter mutation patterns are visible in source

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable domain, no external dependency changes expected)
