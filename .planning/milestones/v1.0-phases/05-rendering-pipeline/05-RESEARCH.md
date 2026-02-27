# Phase 5: Rendering Pipeline - Research

**Researched:** 2026-02-27
**Domain:** Terminal differential rendering, batched I/O, graph character caching (C++17/20)
**Confidence:** HIGH

## Summary

Phase 5 transforms btop's rendering pipeline from "rebuild full output string every frame, flush via cout" to a differential rendering model where only changed cells emit escape sequences, all output is flushed in a single write() syscall, and graph characters are cached when data points have not changed. This is a pure optimization phase -- no UI/UX changes.

The current architecture concatenates per-box draw output into a single `Runner::output` string (typically 50-200KB of escape sequences + Unicode characters per frame), then flushes it through `std::cout << output << flush`. Every frame regenerates escape sequences for every visible cell, even when most content (box borders, labels, graph history) has not changed. The key insight is that during steady-state operation (no resize, no config change), only a thin right-column of each graph and the numeric values (percentages, temperatures, speeds) actually change between frames -- the vast majority of terminal cells are identical.

The approach is to introduce a cell buffer (2D array of {character, fg_color, bg_color, attributes}) that represents the terminal state, diff it against the previous frame, and emit escape sequences only for cells that differ. Output is accumulated into a single pre-allocated string and flushed via `::write(STDOUT_FILENO, buf, len)` instead of iostream. Graph rendering gains an explicit cache for previously-computed braille/tty/block characters, avoiding the shift-and-recompute pattern for unchanged columns.

**Primary recommendation:** Implement a screen-level Cell buffer with double-buffering and per-cell diff, replacing the current string-concatenation draw model. Use POSIX write() for single-syscall output. Add per-column caching to Graph class.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| REND-01 | Differential terminal output -- only emit escape sequences for cells that changed between frames (30-50% draw time reduction target) | Cell buffer double-buffering pattern (see Architecture Pattern 1). FTXUI, ratatui, and notcurses all validate this approach. The UpdatePixelStyle pattern from FTXUI shows how to emit only changed attributes. |
| REND-02 | Terminal output batched into single write() call per frame instead of multiple small writes | Replace `cout << output << flush` with `::write(STDOUT_FILENO, buf, len)` (see Architecture Pattern 2). Current code already concatenates into one string -- the change is to bypass iostream buffering for guaranteed single-syscall output. |
| REND-03 | Graph rendering caches braille/tty characters for unchanged data points | Graph::operator() already has `data_same` early-return. Enhancement: cache per-column symbols in Graph class, skip _create for columns where data hasn't changed, only regenerate the `out` string when the rightmost column is appended (see Architecture Pattern 3). |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| POSIX write() | N/A (system) | Single-syscall terminal output | Guarantees atomicity, bypasses iostream buffering layers. Already used in btop for read_proc_file(). |
| std::array / std::vector | C++17 standard | Cell buffer storage | 2D array of Cell structs. No external dependency needed. |
| fmt::format_to | 10.x (already in project) | Escape sequence generation into cell buffer | Already used throughout btop draw code (Phase 2 migration). Back-inserter pattern avoids intermediate strings. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| nanobench | 4.3.11 (already in project) | Benchmark graph caching and diff overhead | Extend bench_draw.cpp with differential rendering benchmarks |
| strace / dtruss | System tool | Measure bytes written per frame | Verification of REND-01 success criteria |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom cell buffer | FTXUI library | FTXUI is a full TUI framework -- massive dependency for a project that manages its own rendering. btop only needs the diff concept, not the framework. |
| Custom cell buffer | ncurses | ncurses would require rewriting all draw functions to use ncurses API. btop's escape-sequence approach is deliberate and well-established. |
| POSIX write() | writev() scatter-gather | writev() could avoid final string concatenation by writing multiple iovec buffers in one syscall, but the current architecture already builds a single string. write() is simpler and sufficient. |

## Architecture Patterns

### Recommended Changes to Existing Structure
```
src/
├── btop_draw.hpp        # Add Cell struct, ScreenBuffer class declaration
├── btop_draw.cpp        # Existing draw functions adapted to write to cell buffer
├── btop_tools.hpp       # Add write_stdout() helper
├── btop_tools.cpp       # Implement write_stdout() with POSIX write()
└── btop.cpp             # Runner thread: diff buffer + flush via write_stdout()
```

Note: This phase modifies existing files. No new source files should be needed unless the cell buffer abstraction warrants a separate `btop_screen.hpp/cpp` for cleanliness.

### Pattern 1: Double-Buffered Cell Diff (REND-01)

**What:** Maintain two 2D cell buffers (current and previous). Each draw cycle writes to `current`. After all boxes are drawn, diff `current` against `previous`, emit escape sequences only for cells that differ, then swap buffers.

**When to use:** Every normal frame render (not force_redraw).

**Cell structure:**
```cpp
struct Cell {
    char32_t character = U' ';     // Unicode codepoint (or small buffer for multi-byte)
    uint32_t fg_color = 0;         // Packed RGB or terminal color index
    uint32_t bg_color = 0;         // Packed RGB or terminal color index
    uint8_t  attrs = 0;            // Bitfield: bold, dim, italic, underline, blink, etc.

    bool operator==(const Cell&) const = default;  // C++20 defaulted comparison
};
```

**Diff algorithm (adapted from ratatui/FTXUI pattern):**
```cpp
// Source: Pattern derived from ratatui double-buffer diff + FTXUI UpdatePixelStyle
void diff_and_emit(const ScreenBuffer& prev, const ScreenBuffer& curr, string& out) {
    out.clear();
    out.reserve(curr.width() * curr.height() * 4);  // Conservative estimate for diff output

    int last_x = -1, last_y = -1;
    uint32_t last_fg = UINT32_MAX, last_bg = UINT32_MAX;
    uint8_t last_attrs = UINT8_MAX;

    for (int y = 0; y < curr.height(); ++y) {
        for (int x = 0; x < curr.width(); ++x) {
            const auto& c = curr.at(x, y);
            const auto& p = prev.at(x, y);

            if (c == p) continue;  // Skip unchanged cells

            // Emit cursor move only if not sequential
            if (y != last_y || x != last_x + 1) {
                fmt::format_to(std::back_inserter(out), "\x1b[{};{}f", y + 1, x + 1);
            }

            // Emit attribute changes only when different from last emitted
            if (c.attrs != last_attrs) { emit_attr_diff(last_attrs, c.attrs, out); last_attrs = c.attrs; }
            if (c.fg_color != last_fg) { emit_fg(c.fg_color, out); last_fg = c.fg_color; }
            if (c.bg_color != last_bg) { emit_bg(c.bg_color, out); last_bg = c.bg_color; }

            // Emit character
            encode_utf8(c.character, out);

            last_x = x;
            last_y = y;
        }
    }
}
```

**Key insight from FTXUI:** Track the "last emitted" style state across cells. Only emit escape codes when the style actually changes, not for every cell. This is critical because escape codes (e.g., `\x1b[38;2;R;G;Bm`) are 15-20 bytes each -- emitting them unnecessarily doubles output size.

**Integration with existing draw functions:**
The existing `Cpu::draw()`, `Mem::draw()`, etc. currently return strings containing escape sequences + characters mixed together. The migration path is:
1. Draw functions write to the ScreenBuffer (cell-by-cell) instead of appending to a string
2. The Runner thread diffs the buffer and emits the minimal output
3. `force_redraw` / `redraw` flags cause a full buffer write (no diff)

**Alternative (lower-risk) approach:** Rather than converting all draw functions at once, the cell buffer can be populated by *parsing* the existing output strings. This is slower but allows incremental migration -- draw functions continue to produce escape-sequence strings, but instead of writing them to the terminal directly, they are "rendered" into the cell buffer, which is then diffed. This avoids rewriting all draw functions simultaneously.

### Pattern 2: Single write() Output (REND-02)

**What:** Replace `cout << output << flush` with `::write(STDOUT_FILENO, output.data(), output.size())`.

**When to use:** All terminal output paths in Runner::_runner and Runner::run.

```cpp
// Source: POSIX standard, consistent with btop's existing read_proc_file() pattern
#include <unistd.h>

void write_stdout(const std::string& buf) {
    const char* data = buf.data();
    size_t remaining = buf.size();
    while (remaining > 0) {
        ssize_t written = ::write(STDOUT_FILENO, data, remaining);
        if (written < 0) {
            if (errno == EINTR) continue;  // Retry on signal interruption
            break;  // Terminal gone, nothing to do
        }
        data += written;
        remaining -= static_cast<size_t>(written);
    }
}
```

**Current output points to replace (all in btop.cpp):**
- Line 732-735: Main runner output (the primary frame write)
- Line 767: Overlay output
- Line 771: Clock output
- Line 1238: Initial box outlines
- Line 167-183: Terminal too small message (low priority)

**Terminal sync integration:** The sync_start/sync_end sequences (CSI ?2026h / CSI ?2026l) should be prepended/appended to the output buffer before the write() call, not as separate writes.

**cout.sync_with_stdio(false) note:** btop already disables sync_with_stdio (btop_tools.cpp:162). After migration to write(), this is no longer needed but should be left for any remaining cout usage (e.g., error paths).

### Pattern 3: Graph Column Caching (REND-03)

**What:** Cache the per-column braille/block/tty symbol strings in the Graph class. When only the newest data point changes (the common case during steady-state), shift cached columns left, compute only the rightmost new column, and rebuild `out` from cached columns.

**When to use:** Graph::operator() when data_same is false but most historical data points are unchanged.

**Current behavior analysis:**
```
Graph::operator()(data, false):
  1. Toggle current buffer (for braille double-encoding)
  2. Erase leftmost character from each row (lines 498-504) -- O(height) string erases
  3. Call _create(data, data.size()-1) -- computes ONLY the rightmost column
  4. Rebuild `out` from all rows (lines 446-460) -- O(width * height) string concatenation
```

The `_create` call on line 506 already only processes the last data point. The main waste is step 4: rebuilding the entire `out` string from scratch every frame, even though only the rightmost column changed.

**Enhanced approach:**
```cpp
class Graph {
    // ... existing members ...

    // NEW: Cache per-column symbols for each row
    // columns[row][col] = braille/block character (3 bytes max)
    std::vector<std::vector<std::string>> cached_columns;

    // NEW: Flag to track if full rebuild is needed
    bool needs_full_rebuild = true;

    string& operator()(const RingBuffer<long long>& data, bool data_same) {
        if (data_same) return out;

        // Shift columns left (O(height) pointer shifts, not string copies)
        for (auto& row : cached_columns) {
            row.erase(row.begin());
        }

        // Compute new rightmost column
        this->_create(data, (int)data.size() - 1);

        // Cache the new column
        for (int h = 0; h < height; ++h) {
            cached_columns[h].push_back(/* newly computed symbol */);
        }

        // Rebuild out from cached columns (same cost, but enables future optimization)
        // Future: if writing to cell buffer, can diff columns individually
        rebuild_out_from_cache();
        return out;
    }
};
```

**Key optimization opportunity:** When the Graph writes directly to the ScreenBuffer (Pattern 1), only the rightmost column cells need updating. The rest of the graph area in the cell buffer is already correct from the previous frame. This eliminates O(width * height) string operations per graph per frame.

### Anti-Patterns to Avoid

- **Full screen clear + redraw:** Never use Term::clear for normal frames. Only clear on resize or force_redraw. Differential output relies on the previous screen state being known.
- **Parsing escape sequences to populate cell buffer:** While this is a valid migration strategy, it's slower than direct cell writes. The incremental approach (escape-string parser) is acceptable as the phase deliverable IF measurement demonstrates net-positive byte-reduction despite parser overhead. Direct cell writes should be pursued in a future phase if profiling shows the parser is a bottleneck. The key metric is total bytes written per frame, not parser CPU cost in isolation.
- **Cell buffer per box:** Don't create separate cell buffers for each box. Use a single screen-wide buffer. Boxes write to their rectangular region within it.
- **Forgetting terminal state tracking:** After a resize, the cell buffer must be fully invalidated (mark all cells as dirty). The `force_redraw` / `redraw` flag system already handles this.
- **Double-encoding braille in cell buffer:** The Graph class uses a `current` toggle for braille double-encoding (two values per character width). This encoding must be resolved before writing to the cell buffer -- each cell gets exactly one character.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| UTF-8 encoding | Custom UTF-8 encoder | Standard library / existing btop UTF-8 handling | btop already handles UTF-8 throughout. Cell buffer should store char32_t and encode to UTF-8 at output time using existing utilities. |
| Color representation | Custom color space | Existing Theme::hex_to_color / Theme::dec_to_color | btop has a complete theme/color system. Cell buffer stores the terminal-ready color index, not RGB. |
| Terminal capability detection | terminfo/termcap parser | Existing hardcoded ANSI sequences | btop deliberately uses direct ANSI/xterm escape sequences rather than terminfo. Don't change this approach. |

**Key insight:** btop is NOT a general-purpose TUI framework. It targets modern xterm-compatible terminals with known capabilities. The cell buffer only needs to support what btop actually uses: cursor positioning, 256-color/truecolor fg/bg, bold/unbold/dim/italic/underline/blink/strikethrough, and Unicode characters. No need for a generic terminal abstraction.

## Common Pitfalls

### Pitfall 1: Output Size Regression on Force Redraw
**What goes wrong:** Differential output is smaller during steady-state, but force_redraw (resize, config change) produces LARGER output than the current approach because the cell buffer emits per-cell escape sequences instead of the compact draw-function output.
**Why it happens:** Draw functions produce optimized output by sharing color states across adjacent cells implicitly. A naive cell diff emits color codes per-cell.
**How to avoid:** Track "last emitted" fg/bg/attrs across cells during diff output (as shown in Pattern 1). Only emit color changes when they actually differ from the previously emitted state. On force_redraw, the diff is against an empty buffer, so every cell is "changed" -- but the color tracking still minimizes redundant escape codes.
**Warning signs:** Output size on force_redraw is larger than current approach. Measure with strace -e write.

### Pitfall 2: Box Draw Functions Produce Escape Sequences, Not Cell Data
**What goes wrong:** The existing Cpu::draw(), Mem::draw(), etc. return strings full of Mv::to() cursor moves, Fx::b bold codes, Theme::c() color strings, etc. Converting these to direct cell-buffer writes requires understanding and replacing hundreds of string concatenations.
**Why it happens:** btop was designed as a string-based renderer, not a cell-based one. Every draw function assumes it's building a string.
**How to avoid:** Two strategies: (a) Incremental: Create a "string-to-cell-buffer renderer" that parses the escape sequences in the draw output and populates the cell buffer. This is slower but allows gradual migration. (b) Direct: Rewrite draw functions to write cells directly. This is faster but higher risk and more work.
**Recommended approach:** Start with strategy (a) for initial implementation, validate the diff mechanism works, then migrate hot-path draw functions to strategy (b) in subsequent plans.
**Warning signs:** Draw function changes break visual output. Always compare before/after screenshots.

### Pitfall 3: Braille Graph Encoding Mismatch
**What goes wrong:** Graph characters encode two data values per column-width (the `current` toggle in Graph class). If the cell buffer doesn't account for this, graph rendering breaks.
**Why it happens:** The braille character at each position encodes both "current" and "last" values in a 5x5 grid. The Graph class maintains two parallel graph buffers (`graphs[0]` and `graphs[1]`) and alternates between them.
**How to avoid:** Resolve the braille encoding to a single character per cell BEFORE writing to the cell buffer. The existing `_create` method already does this -- the `out` string contains the resolved characters. Use `out` to populate the cell buffer, not the intermediate `graphs[]` arrays.
**Warning signs:** Graph characters appear to "flicker" or show wrong encoding.

### Pitfall 4: Thread Safety of Cell Buffer
**What goes wrong:** The Runner thread writes to the cell buffer while the main thread might read terminal dimensions during resize.
**Why it happens:** btop uses a secondary thread (Runner::_runner) for collection and drawing. The cell buffer is written in this thread and read for diff output.
**How to avoid:** The cell buffer should be owned by the Runner thread, same as the current `Runner::output` string. The double-buffer swap happens within the runner thread under the existing atomic_lock. No additional synchronization is needed if the buffer follows the same ownership pattern as `Runner::output`.
**Warning signs:** Crashes or corruption during resize. Test with rapid SIGWINCH.

### Pitfall 5: macOS/FreeBSD Measurement Differences
**What goes wrong:** Success criteria reference strace for measuring bytes written, but strace is Linux-only.
**Why it happens:** Different platforms have different syscall tracing tools.
**How to avoid:** Use dtruss on macOS, truss on FreeBSD. Alternatively, add a built-in byte counter: wrap write_stdout() with an atomic counter that accumulates bytes written per frame, accessible via --debug mode.
**Warning signs:** Cannot verify REND-01 success criteria on non-Linux platforms.

## Code Examples

### Example 1: Cell Buffer Class
```cpp
// Minimal cell buffer for btop's rendering pipeline
struct Cell {
    // Store the UTF-8 encoded character directly (most btop chars are 1-3 bytes)
    // Using a small string optimization approach
    char ch[4] = {' ', 0, 0, 0};  // UTF-8 encoded, null-terminated
    uint8_t ch_len = 1;

    // Colors stored as terminal-ready escape sequence index
    // 0 = default, 1-255 = 256-color, 256+ = truecolor (packed RGB)
    uint32_t fg = 0;
    uint32_t bg = 0;

    // Attributes bitfield
    uint8_t attrs = 0;  // bit 0=bold, 1=dim, 2=italic, 3=underline, 4=blink, 5=strikethrough

    bool operator==(const Cell& o) const {
        return fg == o.fg && bg == o.bg && attrs == o.attrs
            && ch_len == o.ch_len && std::memcmp(ch, o.ch, ch_len) == 0;
    }
    bool operator!=(const Cell& o) const { return !(*this == o); }
};

class ScreenBuffer {
    std::vector<Cell> cells;
    int w = 0, h = 0;
public:
    void resize(int width, int height) {
        w = width; h = height;
        cells.resize(w * h);
    }
    Cell& at(int x, int y) { return cells[y * w + x]; }
    const Cell& at(int x, int y) const { return cells[y * w + x]; }
    int width() const { return w; }
    int height() const { return h; }
    void clear() { std::fill(cells.begin(), cells.end(), Cell{}); }
};
```

### Example 2: write_stdout Helper
```cpp
// POSIX write() wrapper -- single syscall for frame output
// Handles partial writes and signal interruption
#include <unistd.h>
#include <cerrno>

inline void write_stdout(const char* data, size_t len) {
    while (len > 0) {
        ssize_t n = ::write(STDOUT_FILENO, data, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return;  // Terminal gone or error -- bail
        }
        data += n;
        len -= static_cast<size_t>(n);
    }
}

inline void write_stdout(const std::string& s) {
    write_stdout(s.data(), s.size());
}
```

### Example 3: Replacing cout in Runner Thread
```cpp
// Before (current code, btop.cpp ~line 732):
cout << (term_sync ? Term::sync_start : "") << (conf.overlay.empty()
    ? output
    : (output.empty() ? "" : Fx::ub + Theme::c("inactive_fg") + Fx::uncolor(output)) + conf.overlay)
    << (term_sync ? Term::sync_end : "") << flush;

// After (single write):
string frame_output;
frame_output.reserve(output.size() + 32);  // +32 for sync sequences
if (term_sync) frame_output += Term::sync_start;
if (conf.overlay.empty()) {
    frame_output += output;
} else {
    if (!output.empty()) frame_output += Fx::ub + Theme::c("inactive_fg") + Fx::uncolor(output);
    frame_output += conf.overlay;
}
if (term_sync) frame_output += Term::sync_end;
write_stdout(frame_output);
```

### Example 4: Escape Sequence String to Cell Buffer Parser (Migration Strategy)
```cpp
// Parse an escape-sequence-laden string into the cell buffer
// This enables incremental migration: draw functions keep producing strings,
// but output goes through the cell buffer for diffing
void render_string_to_buffer(ScreenBuffer& buf, const std::string& escape_str) {
    int cx = 0, cy = 0;  // cursor position
    uint32_t fg = 0, bg = 0;
    uint8_t attrs = 0;

    size_t i = 0;
    while (i < escape_str.size()) {
        if (escape_str[i] == '\x1b' && i + 1 < escape_str.size() && escape_str[i+1] == '[') {
            // Parse CSI sequence
            i += 2;
            // ... parse parameters, determine if it's:
            //   - Cursor move (CUP: Ps;Psf or Ps;PsH)
            //   - SGR (color/attribute: Ps...m)
            //   - Cursor relative move (CUF: PsC, CUB: PsD, CUU: PsA, CUD: PsB)
            // Update cx, cy, fg, bg, attrs accordingly
        } else {
            // Regular character -- write to cell buffer
            auto& cell = buf.at(cx, cy);
            // Decode UTF-8 character at position i
            // Set cell.ch, cell.ch_len, cell.fg, cell.bg, cell.attrs
            cx++;
            if (cx >= buf.width()) { cx = 0; cy++; }
        }
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Full screen redraw every frame | Differential cell buffer output | Standard since ncurses (1990s), refined by ratatui/FTXUI/notcurses (2020s) | 30-90% reduction in bytes written per frame during steady state |
| Multiple small write() calls | Single write() per frame + terminal sync (CSI ?2026h) | Terminal sync standardized ~2021 (kitty, iTerm2, VTE) | Eliminates tearing, reduces syscall overhead |
| iostream cout << flush | POSIX write(STDOUT_FILENO) | Always available, modern C++ TUI libs prefer direct write | Bypasses iostream buffering, guarantees single syscall |

**btop's current state:**
- Already has terminal sync support (CSI ?2026h/l) as a config option
- Already concatenates all box outputs into a single string before cout
- Already has `data_same` flag for graph caching (but underutilized)
- Does NOT have a cell buffer or differential output

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Google Test 1.17.0 + nanobench 4.3.11 |
| Config file | tests/CMakeLists.txt, benchmarks/CMakeLists.txt |
| Quick run command | `cmake --build build --target btop_test && ./build/tests/btop_test` |
| Full suite command | `cmake --build build --target btop_test btop_bench_draw && ./build/tests/btop_test && ./build/benchmarks/btop_bench_draw` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| REND-01 | Cell diff emits only changed cells | unit | `./build/tests/btop_test --gtest_filter=ScreenBuffer*` | No -- Wave 0 |
| REND-01 | 30%+ byte reduction in steady-state | smoke (manual) | `strace -e trace=write -c ./build/btop --benchmark 20` (Linux) | N/A (manual measurement) |
| REND-02 | Single write() per frame | smoke (manual) | `strace -e trace=write ./build/btop` -- verify one write per frame | N/A (manual measurement) |
| REND-02 | write_stdout helper correctness | unit | `./build/tests/btop_test --gtest_filter=WriteStdout*` | No -- Wave 0 |
| REND-03 | Graph caching reduces draw time | benchmark | `./build/benchmarks/btop_bench_draw --json` | Yes (extend existing bench_draw.cpp) |
| REND-03 | Cached graph output matches uncached | unit | `./build/tests/btop_test --gtest_filter=GraphCache*` | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build build --target btop_test && ./build/tests/btop_test`
- **Per wave merge:** Full suite + bench_draw
- **Phase gate:** Full suite green + strace byte measurement documented

### Wave 0 Gaps
- [ ] `tests/test_screen_buffer.cpp` -- unit tests for Cell struct, ScreenBuffer class, diff algorithm (covers REND-01)
- [ ] `tests/test_graph_cache.cpp` -- unit tests for Graph column caching producing identical output (covers REND-03)
- [ ] Extend `benchmarks/bench_draw.cpp` -- add benchmarks for: Graph with cached columns, ScreenBuffer diff steady-state, ScreenBuffer diff force-redraw
- [ ] Extend `tests/CMakeLists.txt` -- add new test source files to btop_test target

## Open Questions

1. **Migration strategy: incremental vs. big-bang?**
   - What we know: Incremental (parse escape strings into cell buffer) is lower risk but slower. Direct cell writes are faster but require touching all draw functions simultaneously.
   - What's unclear: How much performance benefit does the incremental approach give vs. the direct approach? Is the escape-string parser fast enough that the diff savings still net positive?
   - Recommendation: Start with REND-02 (single write) and REND-03 (graph caching) as they are independent of the cell buffer. Then implement REND-01 using the incremental approach first. If measurement shows insufficient gains, convert draw functions to direct cell writes in a follow-up plan.

2. **Cell buffer memory overhead**
   - What we know: For a 200x50 terminal, the cell buffer is 10,000 cells. At ~16 bytes/cell, that's ~160KB for two buffers (320KB total).
   - What's unclear: Is this acceptable memory overhead given btop's existing footprint?
   - Recommendation: 320KB is negligible compared to btop's typical 20-40MB RSS. Proceed without concern.

3. **Color representation in Cell**
   - What we know: btop uses truecolor (24-bit RGB), 256-color, and basic 16-color modes depending on config. Theme colors are stored as escape sequence strings.
   - What's unclear: Should Cell store the packed RGB value or the pre-computed escape string?
   - Recommendation: Store packed RGB (uint32_t, with high byte encoding the color mode). Generate escape sequences during diff output. This makes cell comparison cheap (uint32_t ==) vs. string comparison.

4. **Overlay rendering with uncolor()**
   - What we know: When a menu overlay is active, btop renders `Fx::uncolor(output) + overlay`. The uncolor() strips all color codes from the main output, then the overlay is drawn on top.
   - What's unclear: How does this interact with the cell buffer? The uncolored output would need to be rendered to the buffer with a "dim" attribute, then the overlay cells written on top.
   - Recommendation: Handle overlay as a special case: render main output to buffer normally, then apply a "dim/uncolor" pass over all cells, then render overlay into the buffer. The diff then handles the combined result.

## Sources

### Primary (HIGH confidence)
- btop source code: btop_draw.cpp, btop_draw.hpp, btop.cpp, btop_tools.hpp -- direct analysis of current rendering architecture
- FTXUI source (screen.cpp ToString/UpdatePixelStyle) -- verified C++ cell diff implementation pattern
- POSIX write(2) specification -- guaranteed behavior for single-syscall output

### Secondary (MEDIUM confidence)
- [ratatui rendering documentation](https://ratatui.rs/concepts/rendering/under-the-hood/) -- double-buffer diff algorithm description
- [notcurses render man page](https://notcurses.com/notcurses_render.3.html) -- cell elision and damage detection concepts
- [FTXUI GitHub](https://github.com/ArthurSonzogni/FTXUI) -- C++ TUI framework with differential rendering

### Tertiary (LOW confidence)
- WebSearch results for terminal sync (CSI ?2026h) adoption -- broad but not authoritative on specific terminal support

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Uses only POSIX syscalls and existing btop libraries. No new dependencies.
- Architecture: HIGH - Cell buffer diff is a well-established pattern (ncurses, FTXUI, ratatui, notcurses all use it). Direct code analysis of btop's rendering confirms feasibility.
- Pitfalls: HIGH - Identified from direct analysis of btop's draw functions and codebase patterns. Graph encoding complexity identified from reading Graph::_create source.

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable domain -- terminal rendering patterns are mature)
