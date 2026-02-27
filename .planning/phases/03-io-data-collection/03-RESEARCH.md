# Phase 3: I/O & Data Collection - Research

**Researched:** 2026-02-27
**Domain:** POSIX low-level file I/O, /proc filesystem optimization, hash-based data structure lookup, cross-platform system data collection (Linux, macOS, FreeBSD)
**Confidence:** HIGH

## Summary

Phase 3 targets the data collection hot path -- the code that runs every update cycle to read system metrics from the OS. On Linux, the `Proc::collect()` function in `src/linux/btop_collect.cpp` is the dominant cost center. It opens 3-5 `/proc/[pid]/*` files per process per update cycle via `std::ifstream` (which heap-allocates internal buffers), performs O(N) linear scans of `current_procs` vector via `rng::find()` for every PID, and the `readfile()` utility in `btop_tools.cpp` calls `fs::exists()` before opening files (double-stat). With 500 running processes, this means ~2,000+ `ifstream` opens per cycle (each involving `open()` + heap alloc + `close()`), plus ~500 linear scans over a 500-element vector (250,000 comparisons).

The fix strategy is three-pronged: (1) Replace `ifstream` usage in hot paths with POSIX `open()`/`read()`/`close()` using stack-allocated buffers, eliminating heap allocations per file read; (2) Remove the `fs::exists()` guard from `readfile()`, instead relying on the `open()` return code (ENOENT) to detect missing files; (3) Replace the `vector<proc_info> current_procs` linear lookup with an `std::unordered_map<size_t, proc_info>` keyed by PID, turning O(N) lookups into O(1) amortized. For macOS and FreeBSD, the equivalent optimization focuses on reducing redundant sysctl calls and applying the same PID hash-map optimization (both platforms have the identical `rng::find(current_procs, pid)` linear scan pattern).

**Primary recommendation:** Implement a low-level `read_proc_file()` helper that uses `open()/read()/close()` with a fixed stack buffer (typically 4096 bytes, sufficient for all `/proc` virtual files), replace `ifstream` in `Proc::collect()` and sensor reading hot paths, eliminate the `fs::exists()` pre-check in `readfile()`, and convert `current_procs` from `vector` to `unordered_map` across all platform collectors.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| IODC-01 | Linux Proc::collect() ifstream usage replaced with POSIX read() and stack buffers, eliminating 1,500-2,000+ heap-allocating file opens per update | Current code opens 3-5 ifstream instances per PID (comm, cmdline, status, stat, statm) plus 29+ readfile() calls for sensors/battery. POSIX read() with stack buffer eliminates ifstream's internal 4KB heap buffer allocation per open. Stack buffer of 4096 bytes covers all /proc virtual files. |
| IODC-02 | Redundant fs::exists() calls removed from readfile() utility (eliminating double-stat) | readfile() at btop_tools.cpp:553 calls fs::exists() which issues stat() syscall, then opens file (another stat+open). Removing the guard halves syscalls for every readfile() call (29+ per cycle in sensor/battery code alone). |
| IODC-03 | O(N^2) linear PID scan replaced with hash-based PID lookup | current_procs is vector<proc_info> with rng::find() linear scan at btop_collect.cpp:3020 (Linux), osx/btop_collect.cpp:1734 (macOS), freebsd/btop_collect.cpp:1141 (FreeBSD). Also v_contains(found, element.pid) at cleanup (line 3188) uses linear scan of found vector. Both should use unordered_set/unordered_map. |
| IODC-04 | Equivalent I/O optimizations applied to macOS and FreeBSD data collectors | macOS: PID lookup is same rng::find pattern (osx/btop_collect.cpp:1734). No /proc I/O but proc_pidinfo() already uses kernel API. FreeBSD: Same PID lookup pattern (freebsd/btop_collect.cpp:1141), uses kvm_getprocs() kernel API. Both need hash-map PID lookup. macOS has no readfile() usage. FreeBSD has no readfile() in proc path. |
</phase_requirements>

## Standard Stack

### Core

| Library/API | Version | Purpose | Why Standard |
|-------------|---------|---------|--------------|
| POSIX `open()`/`read()`/`close()` | POSIX.1-2008 | Replace ifstream for /proc file reads | Zero heap allocation (uses stack buffer), minimal syscall overhead, available on all target platforms. Already partially used in btop (Term::refresh uses open/close for /dev/tty). **Confidence: HIGH** |
| `std::unordered_map<size_t, proc_info>` | C++20 (already used in btop) | Hash-based PID lookup replacing linear vector scan | O(1) amortized lookup vs O(N) linear. btop already uses unordered_map extensively (uid_user, cpu_old, found_sensors, etc.). **Confidence: HIGH** |
| `std::unordered_set<size_t>` | C++20 (already used in btop) | Replace `vector<size_t> found` for alive-PID tracking | O(1) contains() vs O(N) v_contains() linear scan. btop already uses unordered_set (kernels_procs, dead_procs). **Confidence: HIGH** |
| `std::from_chars()` | C++17 | Parse integers from raw char buffers without string construction | Avoids stoul/stoll string temporaries when parsing /proc fields from raw read() buffers. Already available in btop (charconv included in linux/btop_collect.cpp). **Confidence: HIGH** |

### Supporting

| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `strace -c` | System tool | Count syscalls per update cycle before/after optimization | Verification of IODC-01 success criteria (40%+ syscall reduction) |
| `perf stat` | System tool | Measure cache misses and branch mispredictions | Verify hash-map doesn't regress cache performance vs sorted vector |
| nanobench | 4.3.11 | Micro-benchmark POSIX read vs ifstream | Already available from Phase 1 infrastructure |
| `getdents64` | Linux syscall | Alternative to fs::directory_iterator for /proc scanning | If profiling shows directory iteration is a bottleneck (likely not -- fs::directory_iterator already uses getdents) |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| unordered_map for PID lookup | Sorted vector + binary_search | Sorted vector has better cache locality but O(log N) lookup and O(N) insertion. For 500+ PIDs with frequent insert/remove, unordered_map wins. |
| Stack buffer (4096) | mmap of /proc files | /proc files are virtual -- mmap doesn't help and adds complexity. REQUIREMENTS.md explicitly lists "mmap for /proc files" as out of scope. |
| POSIX read() | pread() | pread() avoids lseek but /proc files are sequential reads from offset 0. No benefit over read(). |
| unordered_map | robin_map / flat_hash_map | Better performance but adds external dependency. btop philosophy is "no heavy dependencies." std::unordered_map is sufficient for ~500 PIDs. |
| from_chars | sscanf / custom parser | from_chars is the modern C++ standard, zero-allocation, and already available. sscanf has overhead. Custom parser risks bugs. |

## Architecture Patterns

### Recommended Project Structure

No new files needed. Changes are in-place modifications:
```
src/
+-- btop_tools.cpp              # Modify readfile() -- remove fs::exists(), add POSIX variant
+-- btop_tools.hpp              # Add read_proc_file() declaration
+-- linux/btop_collect.cpp      # Replace ifstream with POSIX read in hot paths
+-- osx/btop_collect.cpp        # Convert current_procs to hash-map PID lookup
+-- freebsd/btop_collect.cpp    # Convert current_procs to hash-map PID lookup
benchmarks/
+-- bench_proc_collect.cpp      # Add before/after benchmarks for POSIX read vs ifstream
```

### Pattern 1: Stack-Buffer /proc File Reader

**What:** A helper function that opens a /proc file with POSIX `open()`, reads into a stack-allocated buffer, and closes. Returns a `string_view` or length indicator. No heap allocation.

**When to use:** Every /proc file read in the hot path (Proc::collect inner loop, sensor reads, CPU stat reads).

**Example:**
```cpp
// Returns number of bytes read, or -1 on error.
// Buffer is caller-provided (stack-allocated).
inline ssize_t read_proc_file(const char* path, char* buf, size_t buf_size) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, buf_size - 1);
    close(fd);
    if (n >= 0) buf[n] = '\0';
    return n;
}

// Usage in Proc::collect:
char buf[4096];
char path_buf[64];
snprintf(path_buf, sizeof(path_buf), "/proc/%zu/stat", pid);
ssize_t n = read_proc_file(path_buf, buf, sizeof(buf));
if (n <= 0) continue;
// Parse buf directly with from_chars / string_view
```

**Source:** POSIX.1-2008 standard, verified against btop's existing POSIX usage patterns (Term::refresh at btop_tools.cpp:98 uses open/ioctl/close).

### Pattern 2: Eliminate Double-Stat in readfile()

**What:** Remove the `fs::exists()` guard and instead attempt `open()` directly, returning fallback on ENOENT.

**When to use:** The `readfile()` utility function (btop_tools.cpp:553).

**Current code:**
```cpp
string readfile(const std::filesystem::path& path, const string& fallback) {
    if (not fs::exists(path)) return fallback;  // <-- stat() syscall
    string out;
    try {
        std::ifstream file(path);               // <-- open() + stat() + heap alloc
        for (string readstr; getline(file, readstr); out += readstr);
    }
    // ...
}
```

**Optimized code:**
```cpp
string readfile(const std::filesystem::path& path, const string& fallback) {
    string out;
    try {
        std::ifstream file(path);
        if (not file.good()) return fallback;    // open() failed = file doesn't exist
        for (string readstr; getline(file, readstr); out += readstr);
    }
    catch (const std::exception& e) {
        Logger::error("readfile() : Exception when reading {} : {}", path, e.what());
        return fallback;
    }
    return (out.empty() ? fallback : out);
}
```

For hot-path callers (sensor reads), prefer the POSIX `read_proc_file()` pattern instead.

### Pattern 3: Hash-Map PID Lookup

**What:** Replace `vector<proc_info> current_procs` with `unordered_map<size_t, proc_info>` for O(1) PID lookup, and replace `vector<size_t> found` with `unordered_set<size_t>` for O(1) alive-PID tracking.

**When to use:** All platform collectors (Linux, macOS, FreeBSD).

**Current hot-path code (repeated per PID):**
```cpp
// O(N) linear scan -- btop_collect.cpp:3020
auto find_old = rng::find(current_procs, pid, &proc_info::pid);
// O(N) linear scan for dead process cleanup -- btop_collect.cpp:3188
auto eraser = rng::remove_if(current_procs, [&](const auto& element){
    return not v_contains(found, element.pid);  // v_contains is also O(N)
});
```

**Optimized approach:**
```cpp
// O(1) amortized lookup
static std::unordered_map<size_t, proc_info> proc_map;
auto it = proc_map.find(pid);
if (it == proc_map.end()) {
    auto [new_it, _] = proc_map.emplace(pid, proc_info{pid});
    it = new_it;
}
auto& new_proc = it->second;

// O(N) cleanup using O(1) set lookups
static std::unordered_set<size_t> found_set;
// ... populate found_set during iteration ...
std::erase_if(proc_map, [&](const auto& pair) {
    return not found_set.contains(pair.first);
});
```

**Important consideration:** The current code relies on vector ordering for tree generation (`rng::stable_sort`, `rng::equal_range`). Tree view generation happens AFTER collection. The solution must maintain a `vector<proc_info>` (or `vector<proc_info*>`) for the sorted output view while using the map for lookups during collection. Two approaches:

1. **Map + vector of pointers:** `unordered_map` owns the data; build a `vector<proc_info*>` for sorting/tree operations. Requires pointer stability (unordered_map provides this for non-rehashing operations).
2. **Map for lookup, copy to vector:** After collection, dump map values into a vector for sorting. This avoids pointer lifetime issues but adds a copy.

Approach 1 is preferred for performance. Note that `unordered_map` provides reference/pointer stability for existing elements (insertions don't invalidate existing iterators/pointers unless rehash occurs; use `reserve()` to prevent rehashing).

### Pattern 4: Path String Construction Without fs::path

**What:** Avoid `fs::path` operator/ for constructing `/proc/[pid]/stat` paths in the hot loop. Use `snprintf` or `fmt::format_to` into a stack buffer instead.

**When to use:** The inner PID iteration loop in Proc::collect.

**Current code (allocates string per path):**
```cpp
pread.open(d.path() / "stat");    // fs::path concatenation allocates
pread.open(d.path() / "comm");
pread.open(d.path() / "cmdline");
```

**Optimized code:**
```cpp
char path_buf[64];  // "/proc/NNNNNNN/cmdline" fits in 32 chars
const int prefix_len = snprintf(path_buf, sizeof(path_buf), "/proc/%zu/", pid);
// Append suffix directly:
memcpy(path_buf + prefix_len, "stat", 5);
ssize_t n = read_proc_file(path_buf, buf, sizeof(buf));
```

### Anti-Patterns to Avoid

- **Using `mmap()` for /proc files:** /proc files are virtual filesystem entries generated on read. They have no backing store, so mmap provides no benefit and may actually fail or behave unexpectedly for some /proc files. This is explicitly out of scope in REQUIREMENTS.md.
- **Opening an fd once and seeking to re-read:** /proc files reflect instantaneous state at open() time. Re-reading a previously opened fd may not reflect updated values (some do, some don't). Always close and reopen.
- **Replacing ifstream globally:** Only hot-path code needs POSIX read. Cold-path ifstream usage (config loading, theme parsing, one-time init) should be left alone -- the complexity isn't worth it.
- **Using string_view into a stack buffer that goes out of scope:** Stack buffers are only valid in the current scope. If data needs to outlive the function, copy to a string.
- **Breaking the Proc::collect() return type:** The function returns `vector<proc_info>&`. Changing internal storage to unordered_map requires maintaining a vector for the return value. Don't change the public API.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Integer parsing from char buffer | Custom atoi-like function | `std::from_chars()` | Standard, handles errors, no locale overhead, already used in btop via `<charconv>` include |
| Hash map for PIDs | Custom hash table | `std::unordered_map<size_t, proc_info>` | Standard, well-tested, size_t has excellent hash distribution with default hasher |
| /proc file reading | Custom buffered I/O layer | Simple `open()/read()/close()` wrapper | /proc files are small (<4KB); a simple wrapper is all that's needed. No buffering layer required. |
| Path construction | Custom string builder class | `snprintf()` into stack buffer | Simple, fast, no allocation, well-understood |
| Syscall counting for verification | Manual counting | `strace -c -e trace=open,openat,stat,fstat` | Accurate, automated, counts exactly what matters |

**Key insight:** The I/O optimization in this phase is about removing unnecessary layers (ifstream, fs::exists, fs::path) rather than adding new ones. The /proc virtual filesystem serves small, simple text files. The optimal approach is the simplest one: raw POSIX read into a stack buffer.

## Common Pitfalls

### Pitfall 1: Buffer Overflow on /proc Files

**What goes wrong:** Stack buffer too small for a /proc file, causing truncated reads that break parsing.
**Why it happens:** Most /proc/[pid] files are <1KB, but `/proc/[pid]/cmdline` can be up to 4096 bytes (PAGE_SIZE limit on Linux), and `/proc/stat` grows with core count.
**How to avoid:** Use 4096 bytes for per-PID files (matches Linux PAGE_SIZE). For `/proc/stat` (one-time read per cycle, not per-PID), either use a larger buffer (8192+) or fall back to ifstream for that single file.
**Warning signs:** Parsing failures that only appear on systems with many cores or long command lines.

### Pitfall 2: Race Conditions on Process Death

**What goes wrong:** Process dies between `readdir()` and `open(/proc/[pid]/stat)`, causing ENOENT.
**Why it happens:** /proc is a live filesystem reflecting instantaneous process state. Processes can exit at any time.
**How to avoid:** The current code already handles this (check `pread.good()` after open and `continue` on failure). The POSIX version must do the same: check `read_proc_file()` return value and skip PIDs with failed reads. This is not a new issue -- just ensure the existing error handling is preserved.
**Warning signs:** Sporadic "file not found" errors in logs.

### Pitfall 3: Breaking Tree View After Changing current_procs to Hash Map

**What goes wrong:** Tree view generation (lines 3283-3309) depends on vector ordering: `rng::stable_sort`, `rng::equal_range`, index-based iteration. Converting to unordered_map breaks all of these.
**Why it happens:** Tree generation sorts by ppid and iterates in order. unordered_map has no ordering.
**How to avoid:** Maintain dual storage: `unordered_map` for O(1) lookup during collection, then build a sorted `vector` (or `vector<reference_wrapper<proc_info>>`) for tree/sort operations. The map is the "source of truth" for data; the vector is the "view" for display. The function still returns `vector<proc_info>&` as before.
**Warning signs:** Tree view shows incorrect parent-child relationships or crashes on sort.

### Pitfall 4: Forgetting to Handle Partial Reads

**What goes wrong:** `read()` returns fewer bytes than requested but more than 0 (partial read).
**Why it happens:** Signals can interrupt read(), or the file may be larger than the buffer.
**How to avoid:** For /proc files, a single read() call is sufficient -- /proc pseudo-files deliver their entire content in one read if the buffer is large enough. If the file is larger than the buffer, the read is truncated, which is fine for fixed-format files like `/proc/[pid]/stat`. For variable-length files like cmdline, either use a larger buffer or loop on read().
**Warning signs:** Truncated process names or missing fields in stat parsing.

### Pitfall 5: Invalidating Iterators/Pointers During Collection

**What goes wrong:** Adding new entries to unordered_map during iteration over it causes rehash, invalidating iterators.
**Why it happens:** unordered_map may rehash when load factor exceeds threshold during emplace/insert.
**How to avoid:** Call `proc_map.reserve(expected_count)` before the collection loop. The expected count is known from the previous cycle's process count (or from counting /proc directory entries first). Alternatively, collect new PIDs into a separate batch and merge after iteration.
**Warning signs:** Crash or corruption during process collection, especially when process count increases rapidly.

### Pitfall 6: Platform-Specific Gotchas for IODC-04

**What goes wrong:** Applying Linux-specific optimizations to macOS/FreeBSD, or missing platform-specific optimization opportunities.
**Why it happens:** macOS and FreeBSD don't use /proc. Their data collection uses different system APIs (sysctl, proc_pidinfo, kvm_getprocs).
**How to avoid:**
- **macOS:** The main optimization is the hash-map PID lookup (same pattern as Linux). macOS already uses `sysctl(KERN_PROC_ALL)` for bulk process listing and `proc_pidinfo()` for per-process data -- these are already efficient kernel APIs. No ifstream replacement needed.
- **FreeBSD:** Same hash-map optimization. `kvm_getprocs()` returns all process data in one call. No per-process file I/O to optimize.
- **Both:** The `v_contains(found, element.pid)` pattern at dead-process cleanup is identical on all platforms and should use `unordered_set` on all.
**Warning signs:** Trying to replace non-existent ifstream usage on macOS/FreeBSD.

## Code Examples

### Example 1: POSIX read_proc_file() Helper

```cpp
// src/btop_tools.hpp (add to Tools namespace)
#include <fcntl.h>
#include <unistd.h>

// Read a small file (e.g., /proc pseudo-file) into caller-provided buffer.
// Returns bytes read, or -1 on error. Buffer is null-terminated on success.
inline ssize_t read_proc_file(const char* path, char* buf, size_t buf_size) {
    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t total = 0;
    while (static_cast<size_t>(total) < buf_size - 1) {
        ssize_t n = ::read(fd, buf + total, buf_size - 1 - total);
        if (n <= 0) break;  // EOF or error
        total += n;
    }
    ::close(fd);
    buf[total] = '\0';
    return total;
}
```

### Example 2: Replacing ifstream in Proc::collect for /proc/[pid]/stat

```cpp
// Before (current code):
pread.open(d.path() / "stat");
if (not pread.good()) continue;
// ... complex ifstream-based field skipping with ignore() and getline()

// After (POSIX read + direct parsing):
char stat_buf[4096];
snprintf(path_buf + prefix_len, sizeof(path_buf) - prefix_len, "stat");
ssize_t stat_n = read_proc_file(path_buf, stat_buf, sizeof(stat_buf));
if (stat_n <= 0) continue;

// Parse directly from stat_buf using string_view and from_chars
std::string_view stat_view(stat_buf, stat_n);
// Skip to field 3 (state) -- find the closing ')' of the comm field
auto comm_end = stat_view.rfind(')');
if (comm_end == std::string_view::npos || comm_end + 2 >= stat_view.size()) continue;
// State is at comm_end + 2
new_proc.state = stat_view[comm_end + 2];
// Continue parsing fields from comm_end + 4 ...
```

### Example 3: Hash-Map PID Lookup with Vector View

```cpp
// In Proc namespace:
static std::unordered_map<size_t, proc_info> proc_map;
static std::vector<proc_info> current_procs;  // view for sorting/return

// During collection:
static std::unordered_set<size_t> alive_pids;
alive_pids.clear();

for (const auto& d : fs::directory_iterator(Shared::procPath)) {
    // ... pid extraction ...
    alive_pids.insert(pid);

    auto [it, inserted] = proc_map.try_emplace(pid, proc_info{pid});
    auto& new_proc = it->second;
    bool no_cache = inserted;

    // ... read proc data into new_proc ...
}

// Remove dead processes:
std::erase_if(proc_map, [&](const auto& pair) {
    return not alive_pids.contains(pair.first);
});

// Build vector view for sorting and return:
current_procs.clear();
current_procs.reserve(proc_map.size());
for (auto& [pid, info] : proc_map) {
    current_procs.push_back(info);  // copy into vector
}
// Or for zero-copy: use vector<reference_wrapper<proc_info>>
```

### Example 4: Fixing readfile() Double-Stat

```cpp
// src/btop_tools.cpp -- remove fs::exists() guard
string readfile(const std::filesystem::path& path, const string& fallback) {
    string out;
    try {
        std::ifstream file(path);
        if (not file.good()) return fallback;  // replaces fs::exists() check
        for (string readstr; getline(file, readstr); out += readstr);
    }
    catch (const std::exception& e) {
        Logger::error("readfile() : Exception when reading {} : {}", path, e.what());
        return fallback;
    }
    return (out.empty() ? fallback : out);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| ifstream for /proc reads | POSIX open()/read()/close() with stack buffers | Best practice since Linux ~2.6 | Eliminates heap allocation per file read; reduces syscall count by ~50% (no stat before open) |
| string-based integer parsing (stoul/stoll) | std::from_chars (C++17) | Standardized 2017, widespread adoption ~2020 | Zero allocation, no locale, faster error handling |
| Linear vector scan for lookup | unordered_map / unordered_set | Always been available but often overlooked in system monitors | O(1) vs O(N); critical at 500+ processes |
| fs::exists() before open() | Attempt open, check return code | POSIX best practice (EAFP vs LBYL) | Eliminates redundant stat() syscall; prevents TOCTOU race |
| fs::path operator/ for path building | snprintf / fmt::format_to into stack buffer | Performance awareness since C++17 filesystem adoption | fs::path allocates internally; snprintf into stack buffer does not |

**Deprecated/outdated:**
- **ifstream for /proc reads in hot paths:** Creates unnecessary heap allocations. Modern system monitors (htop, btop alternatives) use raw POSIX I/O for /proc.
- **fs::exists() before open():** Classic TOCTOU anti-pattern. The file may disappear between exists() and open(). Direct open() is both safer and faster.

## Open Questions

1. **Optimal buffer size for /proc files**
   - What we know: Most /proc/[pid] files are <1KB. /proc/[pid]/cmdline can be up to ~4KB (PAGE_SIZE). /proc/stat grows linearly with core count (~100 bytes per core).
   - What's unclear: Whether a single 4096-byte buffer is sufficient for all cases, or if /proc/stat on 128+ core machines needs special handling.
   - Recommendation: Use 4096 for per-PID files (comm, stat, cmdline, status, statm). Use 8192 or dynamic allocation for /proc/stat and /proc/meminfo (read once per cycle, not per-PID). These are cold-path reads so ifstream is acceptable as fallback.

2. **Impact of unordered_map on cache performance vs sorted vector**
   - What we know: unordered_map has worse cache locality than vector due to pointer chasing. However, the current code does O(N) linear scan which is far worse than O(1) hash lookup even with cache misses.
   - What's unclear: The exact crossover point where hash-map overhead exceeds linear scan benefit. For very small N (<20), linear scan may be faster.
   - Recommendation: btop typically monitors 200-2000+ processes. At this scale, unordered_map wins overwhelmingly. Profile after implementation to confirm.

3. **Thread safety during proc_map modification**
   - What we know: Proc::collect() is called from a single thread (the Runner thread). The atomic guards (Runner::active, Runner::stopping) prevent concurrent access.
   - What's unclear: Whether any other code path reads current_procs concurrently.
   - Recommendation: Current single-threaded access pattern means no additional synchronization needed. Maintain the existing atomic_lock pattern.

4. **Impact on the detailed process view (_collect_details)**
   - What we know: _collect_details (btop_collect.cpp:2817-2912) uses ifstream to read /proc/[pid]/smaps and /proc/[pid]/io. It also does `rng::find(procs, pid)` for PID lookup.
   - What's unclear: Whether _collect_details should also use POSIX read (it reads potentially large smaps file).
   - Recommendation: Apply POSIX read to /proc/[pid]/io (small file). For smaps, keep ifstream -- smaps can be very large (100KB+) and is only read for the single detailed-view process, not per-PID. Convert the rng::find to map lookup.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 (via FetchContent) + nanobench v4.3.11 (via FetchContent) |
| Config file | `tests/CMakeLists.txt` (existing), `benchmarks/CMakeLists.txt` (existing from Phase 1) |
| Quick run command | `ctest --test-dir build -R bench_proc --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure && build/benchmarks/btop_bench_proc --json > proc_results.json` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| IODC-01 | POSIX read_proc_file() reads /proc files correctly | unit | `ctest --test-dir build -R test_read_proc_file -x` | No -- Wave 0 |
| IODC-01 | Proc::collect produces identical results before/after ifstream replacement | integration | `build/benchmarks/btop_bench_proc` (compare output) | Partial -- bench_proc_collect.cpp exists |
| IODC-01 | strace confirms heap allocs eliminated and syscall count reduced 40%+ | manual | `strace -c -e trace=open,openat,read,close,stat,fstat ./build/btop --benchmark 10 2>&1 \| tail -20` | N/A -- manual verification |
| IODC-02 | readfile() no longer calls fs::exists() | unit | `grep -c 'fs::exists' src/btop_tools.cpp` should show no hit in readfile | N/A -- code review |
| IODC-02 | readfile() still returns fallback for non-existent files | unit | `ctest --test-dir build -R test_readfile -x` | No -- Wave 0 |
| IODC-03 | PID lookup uses hash-based container | unit | `grep 'unordered_map.*proc_info' src/linux/btop_collect.cpp` | N/A -- code review |
| IODC-03 | Process collection time scales linearly with process count | benchmark | `build/benchmarks/btop_bench_proc --json` | Partial -- exists but needs POSIX read comparison |
| IODC-04 | macOS collector uses hash-map PID lookup | unit | `grep 'unordered_map.*proc_info\|rng::find.*current_procs' src/osx/btop_collect.cpp` | N/A -- code review |
| IODC-04 | FreeBSD collector uses hash-map PID lookup | unit | `grep 'unordered_map.*proc_info\|rng::find.*current_procs' src/freebsd/btop_collect.cpp` | N/A -- code review |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build --output-on-failure` (existing tests pass) + `build/benchmarks/btop_bench_proc` (benchmark still runs, no regression)
- **Per wave merge:** Full benchmark suite with `--json` output, compare syscall counts via strace
- **Phase gate:** strace confirms 40%+ syscall reduction; Proc::collect benchmark shows improvement; all existing tests pass; btop runs correctly on all platforms

### Wave 0 Gaps
- [ ] `tests/test_read_proc_file.cpp` -- unit test for new POSIX read helper (covers IODC-01)
- [ ] `tests/test_readfile.cpp` -- unit test for readfile() without fs::exists (covers IODC-02)
- [ ] `benchmarks/bench_proc_collect.cpp` update -- add POSIX read vs ifstream comparison benchmark (covers IODC-01)
- [ ] Existing `bench_proc_collect.cpp` already benchmarks Proc::collect end-to-end

## Sources

### Primary (HIGH confidence)
- btop++ source code -- direct analysis of:
  - `src/btop_tools.cpp` (readfile at line 553, fs::exists guard)
  - `src/btop_tools.hpp` (readfile declaration, existing POSIX includes)
  - `src/linux/btop_collect.cpp` (Proc::collect at line 2915, ifstream usage, rng::find at line 3020, v_contains at line 3188)
  - `src/osx/btop_collect.cpp` (Proc::collect with sysctl/proc_pidinfo, rng::find at line 1734)
  - `src/freebsd/btop_collect.cpp` (Proc::collect with kvm_getprocs, rng::find at line 1141)
  - `src/btop_shared.hpp` (proc_info struct definition, current_procs return type)
- POSIX.1-2008 specification -- open(), read(), close(), O_CLOEXEC behavior
- C++17/20 standard -- std::from_chars, std::unordered_map reference stability guarantees, std::erase_if
- Linux /proc(5) man page -- /proc/[pid]/stat format, virtual filesystem behavior, PAGE_SIZE limits

### Secondary (MEDIUM confidence)
- Phase 1 Research (`01-RESEARCH.md`) -- benchmark infrastructure, nanobench integration, existing bench_proc_collect.cpp
- REQUIREMENTS.md -- explicit "mmap for /proc files" out-of-scope decision; IODC-01 through IODC-04 specifications
- STATE.md -- Phase 3 flagged for deeper macOS/FreeBSD research during planning

### Tertiary (LOW confidence)
- Exact syscall reduction percentage (40%+ target in success criteria): depends on actual process count and system configuration. Must be verified empirically with strace.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all components are POSIX standard or C++ standard library features already used in btop
- Architecture: HIGH -- patterns derived from direct source code analysis with clear transformation paths
- Pitfalls: HIGH -- all pitfalls identified from actual code patterns and /proc filesystem behavior documentation
- Cross-platform (IODC-04): MEDIUM -- macOS and FreeBSD optimizations are primarily hash-map changes (well-understood) but less I/O work to do; the "equivalent optimizations" success criteria may need clarification

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable domain -- POSIX I/O and /proc filesystem don't change)
