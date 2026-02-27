# Phase 4: Data Structure Modernization - Research

**Researched:** 2026-02-27
**Domain:** C++ data structure optimization -- replacing hash maps and deques with contiguous-memory alternatives
**Confidence:** HIGH

## Summary

Phase 4 targets three categories of data structure inefficiency in btop: (1) `unordered_map<string, deque<long long>>` used for time-series metrics in `cpu_info`, `mem_info`, `net_info`, and `gpu_info`, where the key sets are fixed at compile time; (2) `deque<long long>` used for graph history throughout the codebase, which performs heap allocations on every `push_back`/`pop_front` cycle; and (3) `unordered_map` used for small fixed-key lookups in `Config::bools`/`ints`/`strings`, `Symbols::graph_symbols`, `Theme::colors`/`gradients`, and `Draw::Graph::graphs` (a `unordered_map<bool, vector<string>>`).

The project uses C++23 (CMakeLists.txt: `set(CMAKE_CXX_STANDARD 23)`) which means `std::array`, constexpr enum indexing, and all modern standard library features are available. No external libraries are needed -- all replacements use standard C++ facilities. The changes are purely internal to data structures; the public API of collect/draw functions remains unchanged.

**Primary recommendation:** Define enums for every fixed key set (CpuField, MemStat, MemPercent, NetDir, GpuField), replace the `unordered_map<string, ...>` with `std::array<ValueType, EnumCount>` indexed by the enum, and replace all `deque<long long>` time-series with a custom `RingBuffer<long long, N>` template that uses a fixed-size `std::array` internally with zero heap allocation during steady-state operation.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DATA-01 | `unordered_map<string, ...>` with fixed keys replaced by enum-indexed flat arrays (cpu_percent, Config::bools, etc.) | Enum definitions for all key sets identified; all access sites catalogued across 5 platform collectors + draw code; `std::array<T, N>` indexed by enum provides O(1) access with zero hashing |
| DATA-02 | `deque<long long>` for time-series data replaced with fixed-size ring buffers | `width * 2` cap already enforced everywhere via pop_front loops; ring buffer template eliminates heap allocation entirely; `std::array`-backed circular buffer with head/size tracking is sufficient |
| DATA-03 | `map`/`unordered_map` instances replaced with sorted vectors where key sets are small and fixed | `Symbols::graph_symbols` (6 entries), `Draw::Graph::graphs` (2 bool-keyed entries), `Theme::colors`/`gradients`, `Proc::p_graphs`/`p_counters` (PID-keyed, dynamic) all identified; fixed-key maps convert to arrays, dynamic PID-keyed maps remain |
</phase_requirements>

## Standard Stack

### Core

No external libraries needed. All implementations use C++ standard library.

| Facility | Standard | Purpose | Why Standard |
|-----------|----------|---------|--------------|
| `std::array<T, N>` | C++11 | Enum-indexed flat arrays for fixed-key maps | Contiguous memory, O(1) index access, zero overhead, cache-friendly |
| `enum class` + `static_cast<size_t>` | C++11 | Type-safe keys for array indexing | Compile-time safety, documents valid keys, enables `std::array` sizing |
| Custom `RingBuffer<T, N>` template | C++11+ | Fixed-size circular buffer for time-series | No standard ring buffer exists; trivial to implement with `std::array` backing |

### Supporting

| Facility | Standard | Purpose | When to Use |
|-----------|----------|---------|-------------|
| `std::to_underlying()` | C++23 | Convert enum to index cleanly | Prefer over `static_cast<size_t>(e)` for readability |
| `constexpr` | C++11+ | Compile-time enum-to-string mapping tables | For debug/log messages needing key names |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom RingBuffer | `boost::circular_buffer` | Adds external dependency; btop minimizes dependencies; our needs are trivial |
| Custom RingBuffer | `std::deque` with size cap (current) | Current approach; heap allocates on push_back; the whole point of DATA-02 |
| Enum-indexed array | `frozen::unordered_map` (compile-time hash map) | External dependency; array is simpler and faster for known-at-compile-time key sets |
| `std::array<bool, N>` for Config::bools | `std::bitset<N>` | Bitset packs bits; but bool array has simpler access pattern and the 50 bools only use 50 bytes |

## Architecture Patterns

### Pattern 1: Enum-Indexed Array for Fixed-Key Maps

**What:** Replace `unordered_map<string, T>` with `std::array<T, static_cast<size_t>(Enum::COUNT)>` where keys are known at compile time.

**When to use:** The key set is fixed (all keys defined in source code, never added/removed at runtime) and small enough to enumerate.

**Applies to:**
- `Cpu::cpu_info::cpu_percent` -- 11 keys: total, user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice
- `Mem::mem_info::stats` -- 7 keys: used, available, cached, free, swap_total, swap_used, swap_free
- `Mem::mem_info::percent` -- 7 keys (same as stats)
- `Net::net_info::bandwidth` -- 2 keys: download, upload
- `Net::net_info::stat` -- 2 keys: download, upload
- `Gpu::gpu_info::gpu_percent` -- 3 keys: gpu-totals, gpu-vram-totals, gpu-pwr-totals
- `Gpu::shared_gpu_percent` -- 3 keys: gpu-average, gpu-pwr-total, gpu-vram-total
- `Config::bools` -- ~42 keys (fixed set defined in btop_config.cpp)
- `Config::ints` -- ~13 keys
- `Config::strings` -- ~28 keys
- `Symbols::graph_symbols` -- 6 keys (braille_up, braille_down, block_up, block_down, tty_up, tty_down)
- `Draw::Graph::graphs` -- 2 keys (true, false) -- trivially an `array<vector<string>, 2>`

**Example:**

```cpp
// Before:
struct cpu_info {
    std::unordered_map<string, deque<long long>> cpu_percent = {
        {"total", {}}, {"user", {}}, {"nice", {}}, ...
    };
};
// Access: cpu.cpu_percent.at("total").push_back(val);

// After:
enum class CpuField : size_t {
    total, user, nice, system, idle,
    iowait, irq, softirq, steal, guest, guest_nice,
    COUNT
};

struct cpu_info {
    std::array<RingBuffer<long long>, static_cast<size_t>(CpuField::COUNT)> cpu_percent{};
};
// Access: cpu.cpu_percent[std::to_underlying(CpuField::total)].push_back(val);
```

**Key detail:** The enum values must match the `time_names` array ordering used in all 5 platform collectors (Linux, macOS, FreeBSD, OpenBSD, NetBSD). Currently `time_names` is `{"user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal", "guest", "guest_nice"}` on Linux and `{"user", "nice", "system", "idle"}` on BSD/macOS. The enum must include all Linux fields; BSD platforms simply never write to the extra fields.

### Pattern 2: Fixed-Size Ring Buffer

**What:** A circular buffer backed by `std::array<T, N>` that supports `push_back`, `pop_front`, `back()`, `front()`, `size()`, `empty()`, iteration, and indexed access -- all with zero heap allocation.

**When to use:** A `deque<long long>` is used with a known maximum size and the push_back/pop_front pattern.

**Applies to all deque<long long> time-series:**
- `cpu_info::cpu_percent` values (all 11 fields) -- capped at `width * 2`
- `cpu_info::core_percent` entries -- capped at `width * 2`
- `cpu_info::temp` entries -- capped at `width * 2`
- `mem_info::percent` values (7 fields) -- capped at `width * 2`
- `net_info::bandwidth` values (2 fields) -- capped at `width * 2`
- `disk_info::io_read`, `io_write`, `io_activity` -- capped at `width * 2`
- `gpu_info::gpu_percent` values, `temp`, `mem_utilization_percent` -- capped at `width * 2`
- `Proc::detail_container::cpu_percent`, `mem_bytes` -- capped at `width`

**Size determination:** The maximum terminal width btop can reasonably handle is bounded. `Term::width` is the terminal width in columns. The graph data stores `width * 2` entries (because braille characters encode 2 data points per column). For a 500-column terminal (extreme case), that is 1000 entries. A buffer capacity of 4096 would handle any reasonable terminal size with headroom. However, since the capacity is known at graph creation time (from `width * 2`), the ring buffer could accept a runtime capacity up to a compile-time maximum.

**Design choice -- template parameter vs runtime capacity:**
- Template parameter `N`: Zero overhead, but requires all RingBuffers of different sizes to be different types. This complicates containers that hold mixed-capacity buffers.
- Runtime capacity with max template parameter: `RingBuffer<long long, 4096>` always allocates 4096 * 8 = 32KB on the stack. For `cpu_info` with 11 fields, that is 352KB -- too much for stack allocation in the struct.
- **Recommended: Heap-allocate the backing array once at construction, never reallocate.** Use `std::unique_ptr<T[]>` or `std::vector` with `reserve()` and never grow. This gives zero steady-state allocation while keeping the struct size small. The key win is eliminating the per-push_back/pop_front allocation that `deque` performs.

**Revised example:**

```cpp
template<typename T>
class RingBuffer {
    std::unique_ptr<T[]> data_;
    size_t capacity_;
    size_t head_ = 0;  // index of first element
    size_t size_ = 0;
public:
    explicit RingBuffer(size_t capacity = 0)
        : data_(capacity > 0 ? std::make_unique<T[]>(capacity) : nullptr)
        , capacity_(capacity) {}

    void push_back(T value) {
        if (capacity_ == 0) return;
        if (size_ == capacity_) {
            // Overwrite oldest (implicit pop_front)
            data_[(head_ + size_) % capacity_] = value; // overwrites head
            head_ = (head_ + 1) % capacity_;
        } else {
            data_[(head_ + size_) % capacity_] = value;
            ++size_;
        }
    }

    void pop_front() {
        if (size_ > 0) { head_ = (head_ + 1) % capacity_; --size_; }
    }

    T& back() { return data_[(head_ + size_ - 1) % capacity_]; }
    T& front() { return data_[head_]; }
    T& operator[](size_t i) { return data_[(head_ + i) % capacity_]; }
    const T& operator[](size_t i) const { return data_[(head_ + i) % capacity_]; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    size_t capacity() const { return capacity_; }

    // Iterator support needed for Graph::_create which uses iota + data.at(i)
    // and for std::accumulate in net collectors
};
```

### Pattern 3: Bool-Indexed Array for Graph Double-Buffering

**What:** Replace `unordered_map<bool, vector<string>> graphs` in `Draw::Graph` with `std::array<vector<string>, 2> graphs` indexed by `current` (a bool cast to 0/1).

**When to use:** The key is a bool (only two possible values).

```cpp
// Before (btop_draw.hpp line 112):
std::unordered_map<bool, vector<string>> graphs = { {true, {}}, {false, {}}};

// After:
std::array<vector<string>, 2> graphs{};
// Access: graphs[current] (current is already a bool, implicitly converts to 0/1)
```

This is the simplest change in the phase -- a direct type substitution.

### Pattern 4: Config Enum Arrays

**What:** Replace `Config::bools`, `Config::ints`, `Config::strings` (each an `unordered_map<string_view, T>`) with enum-indexed arrays.

**Challenge:** Config keys are used as string_view lookups throughout 12+ files (225 `getB()` calls, 86 `getS()` calls, 72 `getI()` calls). Two migration approaches:

**Approach A -- Enum everywhere (maximum performance):**
```cpp
enum class BoolKey : size_t { theme_background, truecolor, rounded_corners, ..., COUNT };
inline bool getB(BoolKey key) { return bools[std::to_underlying(key)]; }
```
Requires changing all 383 call sites from `Config::getB("name")` to `Config::getB(BoolKey::name)`.

**Approach B -- String-to-enum lookup at API boundary (minimal call-site changes):**
```cpp
// Keep getB(string_view) signature, but internally do:
inline bool getB(const std::string_view name) {
    static const auto map = make_string_to_enum_map<BoolKey>();
    return bools[std::to_underlying(map.at(name))];
}
```
This still hashes on every call -- defeats the purpose.

**Recommended: Approach A.** The 383 call sites are mechanical find-and-replace. The enum names can match the string names exactly. A helper constexpr array mapping enum to string can be provided for serialization (config file read/write).

### Anti-Patterns to Avoid

- **Partial migration leaving both map and array:** Do not keep the `unordered_map` as a fallback alongside the array. This doubles memory and creates inconsistency bugs. Migrate fully per struct.
- **Stack-allocating large ring buffers:** A `RingBuffer<long long, 4096>` as a stack member of `cpu_info` (11 fields * 32KB = 352KB) will overflow the stack. Use heap-allocated backing arrays sized at construction time.
- **Breaking the `safeVal()` contract:** Many draw functions use `safeVal(map, key)` for safe access. With arrays, out-of-bounds access must be similarly guarded. Keep a `safeVal` overload for arrays or use enum typing to make invalid keys a compile error.
- **Forgetting platform-specific field subsets:** Linux has 10 CPU time fields; BSD/macOS has 4. The enum must have all fields but only the relevant ones are populated per platform. Default-initialized (zero) fields for unused entries is correct.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Ring buffer | External library (Boost.CircularBuffer) | Custom `RingBuffer<T>` with `unique_ptr<T[]>` backing | Trivial implementation (~60 lines); avoids new dependency; btop has zero Boost dependency |
| Enum-to-string mapping | Runtime string table with manual sync | `constexpr std::array` of string_view literals, indexed by enum | Compile-time checked, zero runtime cost, auto-stays-in-sync with enum |
| Compile-time hash map | Custom perfect hash | Plain `std::array` indexed by enum | For fixed key sets, an array IS the perfect data structure -- O(1) with no hashing at all |

**Key insight:** For fixed-key maps, the "best hash map" is no hash map. An enum-indexed array is O(1) with zero overhead -- no hash computation, no bucket traversal, no cache misses from pointer chasing, no string comparison. The key sets in btop's data structures (11 CPU fields, 7 mem stats, 2 net directions, 3 GPU fields) are small enough that the array fits in a single cache line or two.

## Common Pitfalls

### Pitfall 1: Iterator Invalidation During Migration

**What goes wrong:** The existing code iterates over `unordered_map` entries with structured bindings: `for (auto& [field, vec] : cpu.cpu_percent)`. After migration to an array, iteration changes to index-based loops.
**Why it happens:** Mechanical replacement of the container type without updating iteration patterns.
**How to avoid:** Search for all `for (auto& [field, vec] :` patterns on each struct. Replace with `for (size_t i = 0; i < array.size(); ++i)` or use an enum iterator helper. Verify all 5 platform collector files.
**Warning signs:** Compilation errors about structured bindings on arrays; or subtler logic errors if field ordering changes.

### Pitfall 2: Deque Random Access vs Ring Buffer Semantics

**What goes wrong:** Existing code accesses `data.at(i)` with absolute indices in `Graph::_create()`. Ring buffer `operator[]` is relative to head.
**Why it happens:** `deque::at(i)` returns the i-th element from the front. Ring buffer `operator[](i)` should also return the i-th element from the front (logical index 0 = oldest). If implemented as `(head_ + i) % capacity_`, this is correct. But the Graph code also uses `data.size()` to compute offsets, which must remain consistent.
**How to avoid:** Ensure RingBuffer's `operator[]` matches deque's zero-based indexing semantics. Write unit tests comparing RingBuffer behavior to a deque for push_back/pop_front/access sequences.
**Warning signs:** Graph rendering shows garbled or offset data after migration.

### Pitfall 3: Config Lock/Tmp Mechanism

**What goes wrong:** Config has a lock mechanism where writes during lock go to `boolsTmp`/`intsTmp`/`stringsTmp` maps and are applied on unlock. Migrating to arrays requires the Tmp arrays to be parallel arrays of the same size, or an alternative mechanism.
**Why it happens:** The current `insert_or_assign` on the Tmp maps is easy with unordered_map; with arrays, you need a bitset tracking which keys were modified.
**How to avoid:** Use `std::optional<T>` for Tmp values: `std::array<std::optional<bool>, BoolCount> boolsTmp{}`. On unlock, apply any non-nullopt values and reset. This preserves the sparse-write semantics.
**Warning signs:** Config changes during locked state being lost or applied incorrectly.

### Pitfall 4: Forgetting to Resize Ring Buffers on Terminal Resize

**What goes wrong:** Terminal resize changes `width`, which changes the required data history length (`width * 2`). Current code handles this via the `pop_front` loop that trims deques to `width * 2`. With a fixed-capacity ring buffer, a resize may require allocating a new buffer of a different capacity.
**Why it happens:** Ring buffers are allocated once at construction with a specific capacity.
**How to avoid:** On terminal resize (`term_resize()` -> `Draw::calcSizes()`), the existing code already clears and rebuilds graphs (`for (auto& [field, vec] : cpu.cpu_percent) { if (vec.size() > width * 2) ... }`). The ring buffer should support a `resize(new_capacity)` operation that preserves the most recent data points. Alternatively, allocate to maximum expected terminal width upfront (e.g., 1024 entries covers 512-column terminals).
**Warning signs:** Data truncation after resize, or ring buffer overflow if terminal grows beyond initial capacity.

### Pitfall 5: Cross-Platform Collector Consistency

**What goes wrong:** The same struct (`cpu_info`, `mem_info`, etc.) is populated by 5 different platform-specific collector files. If the enum definition changes, all 5 collectors must be updated consistently.
**Why it happens:** Each platform file is a separate compilation unit with its own `time_names` array and index loops.
**How to avoid:** Define enums and any enum-to-index helpers in the shared header (`btop_shared.hpp`). Replace each platform's `time_names` string array with the enum values directly. Verify all 5 collectors compile and produce correct data.
**Warning signs:** One platform compiles but produces wrong field assignments; or one platform fails to compile due to missing enum values.

## Code Examples

### Example 1: CPU Field Enum and Migrated cpu_info

```cpp
// In btop_shared.hpp:
enum class CpuField : size_t {
    total, user, nice, system, idle,
    iowait, irq, softirq, steal, guest, guest_nice,
    COUNT  // = 11
};

// Optional: constexpr name table for debug/serialization
inline constexpr std::array<std::string_view,
    static_cast<size_t>(CpuField::COUNT)> cpu_field_names = {
    "total", "user", "nice", "system", "idle",
    "iowait", "irq", "softirq", "steal", "guest", "guest_nice"
};

struct cpu_info {
    std::array<RingBuffer<long long>,
        static_cast<size_t>(CpuField::COUNT)> cpu_percent{};
    vector<RingBuffer<long long>> core_percent;
    vector<RingBuffer<long long>> temp;
    long long temp_max = 0;
    array<double, 3> load_avg;
    float usage_watts = 0;
    std::optional<std::vector<std::int32_t>> active_cpus;
};
```

### Example 2: Mem Stat/Percent Enum

```cpp
enum class MemField : size_t {
    used, available, cached, free,
    swap_total, swap_used, swap_free,
    COUNT  // = 7
};

struct mem_info {
    std::array<uint64_t, static_cast<size_t>(MemField::COUNT)> stats{};
    std::array<RingBuffer<long long>,
        static_cast<size_t>(MemField::COUNT)> percent{};
    std::unordered_map<string, disk_info> disks;  // dynamic keys, stays as map
    vector<string> disks_order;
};
```

### Example 3: Net Direction Enum

```cpp
enum class NetDir : size_t { download, upload, COUNT };

struct net_info {
    std::array<RingBuffer<long long>,
        static_cast<size_t>(NetDir::COUNT)> bandwidth{};
    std::array<net_stat,
        static_cast<size_t>(NetDir::COUNT)> stat{};
    string ipv4{};
    string ipv6{};
    bool connected{};
};
```

### Example 4: Graph Double-Buffer Array

```cpp
// In btop_draw.hpp, Graph class:
// Before:
//   std::unordered_map<bool, vector<string>> graphs = { {true, {}}, {false, {}}};
// After:
    std::array<vector<string>, 2> graphs{};
// Access remains: graphs[current] -- bool implicitly converts to 0 or 1
```

### Example 5: Graph Symbols as Enum-Indexed Array

```cpp
enum class GraphSymbolType : size_t {
    braille_up, braille_down,
    block_up, block_down,
    tty_up, tty_down,
    COUNT  // = 6
};

// In Symbols namespace:
const std::array<vector<string>,
    static_cast<size_t>(GraphSymbolType::COUNT)> graph_symbols = {{
    { " ", "...", "..." },  // braille_up (25 entries)
    { " ", "...", "..." },  // braille_down
    // ...
}};

// Lookup changes from:
//   graph_symbols.at(symbol + '_' + (invert ? "down" : "up"))
// to:
//   graph_symbols[to_graph_symbol_type(symbol, invert)]
// where to_graph_symbol_type maps the string+bool to the enum
```

### Example 6: Collector Migration Pattern

```cpp
// Before (linux/btop_collect.cpp):
cpu.cpu_percent.at("total").push_back(val);
while (cmp_greater(cpu.cpu_percent.at("total").size(), width * 2))
    cpu.cpu_percent.at("total").pop_front();

// After:
auto& buf = cpu.cpu_percent[std::to_underlying(CpuField::total)];
buf.push_back(val);
// No pop_front loop needed -- ring buffer auto-overwrites when full
// (assuming ring buffer capacity was set to width * 2 at construction)
```

### Example 7: safeVal Migration

```cpp
// Before (btop_draw.cpp):
safeVal(cpu.cpu_percent, "total"s)

// After -- with enum, out-of-bounds is a compile error:
cpu.cpu_percent[std::to_underlying(CpuField::total)]
// No safeVal needed -- enum values are always valid indices.
// For the rare dynamic string -> enum conversion (e.g., graph_field from config),
// use a lookup function that returns std::optional<CpuField>.
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `std::map` for config | `std::unordered_map` in btop | Pre-optimization | O(log n) -> O(1) amortized, but still hash overhead |
| `unordered_map` for fixed keys | Enum-indexed `std::array` | C++11 onwards (best practice) | Zero hash cost, cache-line friendly, compile-time safety |
| `std::deque` for bounded queues | Custom ring buffer / `boost::circular_buffer` | Long-standing pattern | Eliminates per-operation heap alloc; contiguous memory for better cache behavior |
| `unordered_map<bool, T>` | `std::array<T, 2>` | Always applicable | Eliminates absurd overhead of hashing a bool |

**Deprecated/outdated:**
- Using `std::deque` as a bounded ring buffer is a well-known anti-pattern when the max size is known. deque's internal block allocation strategy means every push_back can trigger a heap allocation, and pop_front may or may not deallocate the freed block depending on the implementation.

## Open Questions

1. **Ring buffer capacity strategy for terminal resize**
   - What we know: Current deques are trimmed to `width * 2` via pop_front loops. Terminal resize changes width. Ring buffer capacity is set at construction.
   - What's unclear: Whether to (a) allocate to max reasonable width upfront (wastes memory for small terminals), (b) reallocate on resize (one-time allocation, preserving recent data), or (c) destroy and recreate ring buffers on resize (losing history).
   - Recommendation: Option (b) -- provide a `resize(new_capacity)` method. The existing code already rebuilds graphs on resize, so the data loss from option (c) would also be acceptable. Allocating to width * 2 + some headroom at init time and reallocating only when the terminal grows beyond capacity is the pragmatic choice.

2. **Config enum migration scope**
   - What we know: Config has 383 getB/getI/getS call sites. Enum migration requires touching all of them.
   - What's unclear: Whether Config enum migration belongs in Phase 4 or should be deferred. It is the highest call-site-count change and Config lookup is not in any hot path (config values are read at the start of each draw cycle, not per-pixel).
   - Recommendation: Include Config in Phase 4 as DATA-01 explicitly requires it ("Config::bools, etc."). But schedule it as a separate wave from the time-series struct changes, since it touches different files and has different risk profile.

3. **Dynamic-key maps that must remain**
   - What we know: `Mem::mem_info::disks` (keyed by mount path), `Net::current_net` (keyed by interface name), `Proc::p_graphs`/`p_counters` (keyed by PID), and `Theme::colors`/`gradients`/`rgbs` (keyed by theme color names) have dynamic or user-defined key sets.
   - What's unclear: Whether Theme colors could also be enumerated (they come from theme files, but the set of color names is fixed in the code).
   - Recommendation: Leave `disks`, `current_net`, `p_graphs`/`p_counters` as `unordered_map` -- their key sets are genuinely dynamic. Theme colors COULD be enumerated (the color names are fixed in btop_theme.cpp's `setTheme()`), but this is lower priority than the time-series structs. Flag for Phase 4 if time permits, or defer.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Google Test v1.17.0 |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && ctest --test-dir . -R btop_test --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DATA-01 | Enum-indexed arrays replace unordered_map for fixed-key structs | unit | `cd build && ctest -R btop_test --output-on-failure` | No -- Wave 0 |
| DATA-01 | Cachegrind confirms improved cache hit rate | manual + bench | `valgrind --tool=cachegrind ./build-bench/btop --benchmark 50` | No -- manual verification |
| DATA-02 | Ring buffer replaces deque for time-series | unit | `cd build && ctest -R btop_test --output-on-failure` | No -- Wave 0 |
| DATA-02 | Zero heap allocations during steady-state | bench | `cd build-bench && ./btop_bench_draw --json` | Exists (bench_draw.cpp) -- needs ring buffer variants |
| DATA-03 | Small fixed-key maps use flat arrays | unit + bench | `cd build-bench && ./btop_bench_draw --json` | Partially exists |

### Sampling Rate

- **Per task commit:** `cd build && cmake --build . && ctest --output-on-failure` (compile + unit tests)
- **Per wave merge:** `cd build-bench && cmake --build . && ./btop_bench_draw && ./btop_bench_strings` (benchmarks)
- **Phase gate:** Full suite green + `btop --benchmark 50` produces valid output + nanobench confirms speedup

### Wave 0 Gaps

- [ ] `tests/test_ring_buffer.cpp` -- unit tests for RingBuffer<T> template (push_back, pop_front, overflow, iteration, resize, empty/full states)
- [ ] `tests/test_enum_arrays.cpp` -- unit tests verifying enum-indexed array access matches old unordered_map behavior for all key sets
- [ ] Update `benchmarks/bench_draw.cpp` -- add Graph benchmarks comparing deque-backed vs ring-buffer-backed graph updates
- [ ] Update `tests/CMakeLists.txt` -- add new test source files

## Sources

### Primary (HIGH confidence)

- Direct codebase analysis of `btop_shared.hpp` (struct definitions), `btop_config.hpp`/`.cpp` (Config maps), `btop_draw.hpp`/`.cpp` (Graph class, graph_symbols), and all 5 platform collector files (Linux, macOS, FreeBSD, OpenBSD, NetBSD)
- `.planning/phases/03.1-profiling-gap-closure/PROFILING.md` -- CPU profiling data showing Draw::Graph::_create at 2.0% active CPU and Graph construction at 7,529 ns
- C++23 standard library documentation for `std::array`, `std::to_underlying`, enum class semantics

### Secondary (MEDIUM confidence)

- Ring buffer implementation patterns from established C++ practice (Boost.CircularBuffer API as reference for interface design, though not used as dependency)

### Tertiary (LOW confidence)

- None. All findings verified directly against the codebase.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- No external libraries; all std C++ features verified available (C++23)
- Architecture: HIGH -- All target data structures examined in source; enum key sets fully enumerated from code
- Pitfalls: HIGH -- All access patterns traced across all platform collectors and draw code

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable domain; no external dependency version concerns)
