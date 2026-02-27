# Phase 6: Compiler & Verification - Research

**Researched:** 2026-02-27
**Domain:** Profile-Guided Optimization (PGO), sanitizer verification (ASan/TSan/UBSan), alternative allocator evaluation (mimalloc)
**Confidence:** HIGH

## Summary

Phase 6 is the final optimization phase, applying compiler-level techniques (PGO) to the already-optimized btop codebase, then verifying correctness of all prior optimization work through comprehensive sanitizer sweeps, and evaluating mimalloc as an alternative allocator. Unlike Phases 2-5 which modified source code, this phase primarily adds CMake build infrastructure, runs verification tools, and benchmarks external components.

The project already has LTO enabled for release builds (`BTOP_LTO` option, defaults ON for Release/RelWithDebInfo). PGO is a natural complement -- it uses runtime profiling data to guide branch prediction, function layout, and inlining decisions. btop already has a `--benchmark N` mode that runs N collect+draw cycles without terminal initialization, which is an ideal PGO training workload since it exercises all four collectors (CPU, Mem, Net, Proc) and all four draw functions -- the exact hot paths that PGO should optimize.

The sanitizer sweep is a verification gate, not an optimization. btop uses a runner thread (pthread-based) with atomic_lock synchronization against the main thread, plus mutex-protected logging. TSan will validate this concurrency model. ASan will catch any memory errors introduced by the POSIX I/O rewrites (Phase 3) and buffer management (Phase 5). UBSan will catch any undefined behavior from the enum-indexed array access (Phase 4) and integer operations throughout.

**Primary recommendation:** Implement PGO as two new CMake options (BTOP_PGO_GENERATE, BTOP_PGO_USE) that set compiler-specific flags for both Clang and GCC. Use `btop --benchmark 50` as the PGO training workload. Add sanitizer CMake options for all three sanitizers. Benchmark mimalloc via LD_PRELOAD/DYLD_INSERT_LIBRARIES with hyperfine comparing against the default allocator.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BILD-01 | Profile-Guided Optimization (PGO) CMake workflow implemented (10-20% gain target) | CMake PGO pattern with BTOP_PGO_GENERATE/BTOP_PGO_USE options. Clang uses `-fprofile-generate`/`-fprofile-use` (IR PGO, preferred over frontend PGO). GCC uses `-fprofile-generate`/`-fprofile-use`. LTO+PGO are complementary and already coexist in the LLVM/GCC pipelines. Training workload: `btop --benchmark 50`. Measurement: hyperfine comparing PGO vs non-PGO optimized builds. |
| BILD-02 | Full ASan/TSan/UBSan sanitizer sweep completed verifying no correctness regressions from optimizations | Three separate sanitizer build configurations via CMake options. ASan+UBSan can share a build (`-fsanitize=address,undefined`). TSan requires a separate build (`-fsanitize=thread`). Exercise via existing test suite (`ctest`) plus `btop --benchmark 20` to cover runtime paths. CI: add sanitizer job to cmake-linux.yml workflow. |
| BILD-03 | mimalloc evaluated as drop-in allocator replacement, with benchmark comparison against default allocator | mimalloc v3.2.8 available via brew/apt. Linux: `LD_PRELOAD=/usr/lib/libmimalloc.so ./btop --benchmark 50`. macOS: `DYLD_INSERT_LIBRARIES=/usr/local/lib/libmimalloc.dylib ./btop --benchmark 50`. Compare with hyperfine. Document results in RESULTS.md. |
</phase_requirements>

## Standard Stack

### Core
| Tool/Library | Version | Purpose | Why Standard |
|-------------|---------|---------|--------------|
| CMake PGO options | CMake 3.25+ (project minimum) | Build infrastructure for PGO generate/use phases | CMake's `target_compile_options`/`target_link_options` with compiler-detection guards. No external module needed. |
| llvm-profdata | Matches Clang version | Merge .profraw files into .profdata for Clang PGO | Required by Clang's IR PGO pipeline. GCC uses gcov-based .gcda files directly (no merge step). |
| hyperfine | Latest stable | Statistical comparison of PGO vs non-PGO builds | Industry-standard CLI benchmarking tool. Provides warmup runs, statistical outlier detection, and comparison output. |
| ASan/TSan/UBSan | Built into Clang/GCC | Correctness verification of optimized code | Compiler-integrated sanitizers -- no external dependency. Just compiler flags. |
| mimalloc | 3.2.8 (stable) | Alternative allocator evaluation | Microsoft's high-performance allocator. Drop-in via LD_PRELOAD, no code changes needed. |

### Supporting
| Tool/Library | Version | Purpose | When to Use |
|-------------|---------|---------|-------------|
| btop --benchmark N | Built-in | PGO training workload and benchmark measurement | Used as the representative workload for PGO profile collection and for hyperfine timing comparisons |
| ctest | Built into CMake | Run existing test suite under sanitizers | First sanitizer check -- catches issues in unit-testable code paths |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| IR PGO (-fprofile-generate) | Frontend PGO (-fprofile-instr-generate) | Frontend PGO has more profile accuracy loss from optimizations applied before instrumentation. IR PGO is recommended by LLVM for optimization work. Frontend PGO is better for code coverage. Use IR PGO. |
| hyperfine | nanobench macro-benchmark | nanobench already exists in the project but measures internal timing. hyperfine measures end-to-end wall clock time including startup, which is what PGO optimizes. Use hyperfine for PGO comparison, nanobench for micro-benchmarks. |
| mimalloc | jemalloc / tcmalloc | All three are viable. mimalloc is specified in BILD-03. It has the lightest integration (smallest library, simplest LD_PRELOAD usage). Note: mimalloc was historically slower than macOS libmalloc for large allocations but v2.1.4+ resolved this. |
| Separate ASan and UBSan builds | Combined ASan+UBSan build | ASan and UBSan are compatible in the same build. Combining them reduces build count from 3 to 2 (ASan+UBSan, TSan). TSan is incompatible with ASan and must be separate. |

**Installation:**
```bash
# macOS (development)
brew install hyperfine mimalloc

# Ubuntu 24.04 (CI)
sudo apt-get install -y hyperfine libmimalloc-dev
# Note: hyperfine may need to be installed from GitHub releases on Ubuntu
```

## Architecture Patterns

### Pattern 1: Two-Phase PGO CMake Workflow

**What:** Add BTOP_PGO_GENERATE and BTOP_PGO_USE CMake options that inject the correct compiler flags for both Clang and GCC. The build process becomes: (1) configure with PGO_GENERATE, (2) build instrumented binary, (3) run training workload, (4) merge profiles, (5) configure with PGO_USE pointing to profile data, (6) build optimized binary.

**When to use:** Always for PGO builds.

**Clang flags (IR PGO -- preferred):**
```cmake
# Generate phase
if(BTOP_PGO_GENERATE)
  target_compile_options(libbtop PUBLIC -fprofile-generate)
  target_link_options(libbtop PUBLIC -fprofile-generate)
  target_compile_options(btop PUBLIC -fprofile-generate)
  target_link_options(btop PUBLIC -fprofile-generate)
endif()

# Use phase
if(BTOP_PGO_USE)
  if(NOT EXISTS "${BTOP_PGO_USE}")
    message(FATAL_ERROR "PGO profile not found: ${BTOP_PGO_USE}")
  endif()
  target_compile_options(libbtop PUBLIC -fprofile-use=${BTOP_PGO_USE})
  target_link_options(libbtop PUBLIC -fprofile-use=${BTOP_PGO_USE})
  target_compile_options(btop PUBLIC -fprofile-use=${BTOP_PGO_USE})
  target_link_options(btop PUBLIC -fprofile-use=${BTOP_PGO_USE})
endif()
```

**GCC flags:**
```cmake
# GCC uses the same -fprofile-generate/-fprofile-use flags
# but writes .gcda files to the object file directory
# No merge step needed -- GCC reads .gcda files directly
```

**Key insight:** Both Clang and GCC accept `-fprofile-generate` and `-fprofile-use`. Clang's IR PGO (`-fprofile-generate`) is the recommended path for optimization (not `-fprofile-instr-generate` which is for code coverage). The flag names are identical between compilers, but the profile file formats differ:
- **Clang:** Generates `default_NNNN.profraw` files, needs `llvm-profdata merge` to produce `.profdata`
- **GCC:** Generates `.gcda` files alongside `.gcno` files, used directly with `-fprofile-use`

**Training workload:**
```bash
# After building with PGO_GENERATE:
./build-pgo-gen/btop --benchmark 50

# For Clang: merge profiles
llvm-profdata merge -output=btop.profdata build-pgo-gen/default_*.profraw

# For GCC: no merge needed, .gcda files are in the build directory
```

### Pattern 2: Sanitizer CMake Options

**What:** Add BTOP_SANITIZER CMake option that accepts "address", "thread", or "undefined" (or combinations). Sets appropriate compiler and linker flags.

**When to use:** For verification builds. Not for release builds.

**Example:**
```cmake
option(BTOP_SANITIZER "Enable sanitizer (address, thread, undefined, address;undefined)" "")

if(BTOP_SANITIZER)
  foreach(sanitizer IN LISTS BTOP_SANITIZER)
    target_compile_options(libbtop PUBLIC -fsanitize=${sanitizer})
    target_link_options(libbtop PUBLIC -fsanitize=${sanitizer})
    target_compile_options(btop PUBLIC -fsanitize=${sanitizer})
    target_link_options(btop PUBLIC -fsanitize=${sanitizer})
  endforeach()
  # Always include debug info and frame pointers for useful stack traces
  target_compile_options(libbtop PUBLIC -g -fno-omit-frame-pointer)
  target_compile_options(btop PUBLIC -g -fno-omit-frame-pointer)
endif()
```

**Usage:**
```bash
# ASan + UBSan (compatible, can combine)
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DBTOP_SANITIZER="address;undefined"

# TSan (must be separate -- incompatible with ASan)
cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DBTOP_SANITIZER="thread"
```

### Pattern 3: mimalloc LD_PRELOAD Benchmarking

**What:** Use the OS dynamic linker to intercept malloc/free calls with mimalloc, without any code changes or recompilation.

**When to use:** For allocator evaluation only.

**Linux:**
```bash
# Install mimalloc
sudo apt-get install -y libmimalloc-dev
# or build from source for latest version

# Benchmark comparison
hyperfine \
  --warmup 3 \
  --min-runs 10 \
  './btop --benchmark 50' \
  'LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libmimalloc.so ./btop --benchmark 50' \
  --export-json mimalloc_results.json
```

**macOS:**
```bash
# Install mimalloc
brew install mimalloc

# Note: SIP may restrict DYLD_INSERT_LIBRARIES for system binaries
# btop is a user-built binary so this should work
hyperfine \
  --warmup 3 \
  --min-runs 10 \
  './btop --benchmark 50' \
  'DYLD_INSERT_LIBRARIES=$(brew --prefix mimalloc)/lib/libmimalloc.dylib ./btop --benchmark 50' \
  --export-json mimalloc_results.json
```

### Anti-Patterns to Avoid
- **Running sanitizers on optimized builds:** Sanitizers should run on Debug or RelWithDebInfo builds with `-O1` or `-O0`. Highly optimized code can confuse sanitizer diagnostics with inlining and dead code elimination. Use `-O1` for a good balance.
- **Combining TSan with ASan:** These are fundamentally incompatible. TSan intercepts atomic operations differently from ASan. Always use separate builds.
- **PGO training with non-representative workload:** The `--benchmark` mode exercises collect+draw but NOT the terminal I/O path, the input handling path, or the menu system. This is actually fine -- those paths are not hot. The collect+draw loop is where 95%+ of CPU time is spent.
- **Forgetting to rebuild after profile collection:** PGO requires a complete rebuild with `-fprofile-use` pointing to the collected profiles. You cannot just relink.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| PGO profile merging | Custom profile merge scripts | llvm-profdata (Clang) / gcov infrastructure (GCC) | Profile formats are compiler-internal, tools handle versioning |
| Statistical benchmarking | Manual timing scripts | hyperfine | Handles warmup, outlier detection, comparison, and statistical significance |
| Allocator replacement | Custom malloc wrapper | LD_PRELOAD/DYLD_INSERT_LIBRARIES with mimalloc | OS dynamic linker handles symbol interposition transparently |
| Sanitizer flag management | Manual CXXFLAGS hacking | CMake options with target_compile_options | Ensures both compile and link flags are consistent |

**Key insight:** This phase is infrastructure-heavy, not code-heavy. The tools exist and are mature -- the work is wiring them into the CMake build system and CI pipeline correctly.

## Common Pitfalls

### Pitfall 1: PGO Profile Stale After Source Changes
**What goes wrong:** Profiles collected from an older version of the code are used with a newer version. Clang/GCC will emit warnings about mismatched profiles and fall back to non-PGO optimization for affected functions.
**Why it happens:** PGO profiles are tied to the exact control flow graph of the instrumented binary.
**How to avoid:** Always regenerate profiles from the same commit that will be built with PGO. The CMake workflow should make profile generation a documented step, not a cached artifact. For CI, the PGO generate and use phases must happen in the same job.
**Warning signs:** Compiler warnings like "profile data may be out of date" or "no profile data available for function X".

### Pitfall 2: LTO + PGO Flag Ordering
**What goes wrong:** PGO flags must be on both compile AND link lines. With LTO, the link step does significant optimization work and needs the profile data.
**Why it happens:** CMake's `target_compile_options` only affects compilation. If you forget `target_link_options`, the LTO optimizer at link time won't see the profile.
**How to avoid:** Always set both `target_compile_options` AND `target_link_options` for PGO flags. The existing BTOP_LTO handling (`INTERPROCEDURAL_OPTIMIZATION ON`) uses CMake's IPO support which handles this correctly, but PGO flags need explicit link options.
**Warning signs:** PGO build shows no measurable improvement -- likely the link-time optimizer is not using the profile.

### Pitfall 3: TSan False Positives with Custom Atomic Patterns
**What goes wrong:** btop uses a custom `atomic_lock` class and `atomic_wait`/`atomic_wait_for` helpers that implement a bespoke synchronization protocol. TSan may not recognize these as proper synchronization points.
**Why it happens:** TSan understands standard C++ synchronization primitives (mutex, condition_variable, atomic operations with proper memory ordering). btop's `atomic_lock` uses `compare_exchange_strong` and `store` with relaxed ordering plus manual `notify_all`.
**How to avoid:** If TSan reports data races through `atomic_lock`, first verify whether they are true races or false positives. The relaxed memory ordering in btop's atomics is intentional (performance) but may need annotations. TSan supports `__tsan_acquire` / `__tsan_release` annotations if needed.
**Warning signs:** Data race reports involving `Runner::active`, `Config::locked`, or `Config::writelock`.

### Pitfall 4: ASan on macOS with Homebrew Clang
**What goes wrong:** ASan may fail to allocate shadow memory or hang on Apple Silicon when using Homebrew's Clang.
**Why it happens:** macOS's address space layout and SIP restrictions can interfere with ASan's shadow memory mapping, especially with non-system toolchains.
**How to avoid:** Use Apple's system Clang (which comes with Xcode) for macOS sanitizer builds, or do sanitizer sweeps on Linux CI (ubuntu-24.04) where all sanitizers work reliably. The CI already uses ubuntu-24.04 and installs Clang from llvm.sh.
**Warning signs:** "Shadow memory range is not available" errors, hangs on startup.

### Pitfall 5: mimalloc Slower on macOS for Large Allocations
**What goes wrong:** mimalloc v2.0.x was documented as slower than macOS's libmalloc for allocations > 1KB.
**Why it happens:** macOS's libmalloc (magazine allocator) is highly tuned for the platform. mimalloc's performance characteristics differ by allocation size.
**How to avoid:** Use mimalloc v3.2.8 (latest stable) which resolved many macOS regressions. More importantly, btop's allocation profile is dominated by small string allocations (SSO range), which is mimalloc's strength. The benchmark will reveal the actual impact. Accept that mimalloc may show no improvement or even regression on macOS -- that is a valid benchmark result.
**Warning signs:** macOS mimalloc results showing higher wall time than default allocator.

### Pitfall 6: Sanitizer Runtime Overhead Misinterpreted as Performance Issue
**What goes wrong:** ASan adds ~2x overhead, TSan adds ~5-15x overhead. If someone runs benchmarks on a sanitizer build, the results will be misleading.
**Why it happens:** Sanitizer instrumentation adds runtime checks for every memory access (ASan) or synchronization operation (TSan).
**How to avoid:** Never mix sanitizer builds with performance benchmarks. Sanitizer builds are for correctness verification only. Keep separate build directories: `build-release` (performance), `build-asan` (correctness), `build-tsan` (concurrency).
**Warning signs:** Someone comparing sanitizer-build timing against release-build timing.

## Code Examples

### Example 1: Complete PGO CMake Options
```cmake
# Source: Adapted from LLVM PGO documentation + GCC PGO documentation
# Add after the existing BTOP_LTO option block

option(BTOP_PGO_GENERATE "Build with PGO instrumentation" OFF)
set(BTOP_PGO_USE "" CACHE FILEPATH "Path to PGO profile data for optimized build")

if(BTOP_PGO_GENERATE AND BTOP_PGO_USE)
  message(FATAL_ERROR "Cannot set both BTOP_PGO_GENERATE and BTOP_PGO_USE")
endif()

if(BTOP_PGO_GENERATE)
  message(STATUS "PGO: Instrumentation build (generate profiles)")
  target_compile_options(libbtop PUBLIC -fprofile-generate)
  target_link_options(libbtop PUBLIC -fprofile-generate)
  target_compile_options(btop PUBLIC -fprofile-generate)
  target_link_options(btop PUBLIC -fprofile-generate)
endif()

if(BTOP_PGO_USE)
  if(NOT EXISTS "${BTOP_PGO_USE}")
    message(FATAL_ERROR "PGO profile not found: ${BTOP_PGO_USE}")
  endif()
  message(STATUS "PGO: Optimized build using profile: ${BTOP_PGO_USE}")
  target_compile_options(libbtop PUBLIC -fprofile-use=${BTOP_PGO_USE})
  target_link_options(libbtop PUBLIC -fprofile-use=${BTOP_PGO_USE})
  target_compile_options(btop PUBLIC -fprofile-use=${BTOP_PGO_USE})
  target_link_options(btop PUBLIC -fprofile-use=${BTOP_PGO_USE})
endif()
```

### Example 2: Complete PGO Build Sequence (Shell Script)
```bash
#!/bin/bash
# Source: Adapted from LLVM and GCC PGO documentation
set -euo pipefail

BUILD_TYPE="${1:-Release}"
COMPILER="${CXX:-clang++}"

# Step 1: Build instrumented binary
cmake -B build-pgo-gen -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_CXX_COMPILER="$COMPILER" \
  -DBTOP_PGO_GENERATE=ON \
  -DBTOP_LTO=ON
cmake --build build-pgo-gen

# Step 2: Run training workload
./build-pgo-gen/btop --benchmark 50

# Step 3: Merge profiles (Clang only)
if "$COMPILER" --version 2>&1 | grep -q clang; then
  llvm-profdata merge -output=btop.profdata build-pgo-gen/default_*.profraw
  PROFILE_PATH="$(pwd)/btop.profdata"
else
  # GCC: profiles are .gcda files in the build directory
  PROFILE_PATH="$(pwd)/build-pgo-gen"
fi

# Step 4: Build optimized binary with profiles
cmake -B build-pgo-use -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_CXX_COMPILER="$COMPILER" \
  -DBTOP_PGO_USE="$PROFILE_PATH" \
  -DBTOP_LTO=ON
cmake --build build-pgo-use

# Step 5: Benchmark comparison
hyperfine \
  --warmup 3 \
  --min-runs 10 \
  --command-name 'without-pgo' './build-pgo-gen/../btop --benchmark 50' \
  --command-name 'with-pgo' './build-pgo-use/btop --benchmark 50' \
  --export-json pgo_benchmark.json
```

### Example 3: Sanitizer CMake Configuration
```cmake
# Source: Adapted from stablecoder.ca/2018/02/01/analyzer-build-types + LLVM docs
set(BTOP_SANITIZER "" CACHE STRING "Enable sanitizer (address, thread, undefined)")

if(BTOP_SANITIZER)
  message(STATUS "Sanitizer enabled: ${BTOP_SANITIZER}")

  string(REPLACE ";" "," SANITIZER_FLAGS "${BTOP_SANITIZER}")

  target_compile_options(libbtop PUBLIC
    -fsanitize=${SANITIZER_FLAGS}
    -fno-omit-frame-pointer
    -g
  )
  target_link_options(libbtop PUBLIC -fsanitize=${SANITIZER_FLAGS})
  target_compile_options(btop PUBLIC
    -fsanitize=${SANITIZER_FLAGS}
    -fno-omit-frame-pointer
    -g
  )
  target_link_options(btop PUBLIC -fsanitize=${SANITIZER_FLAGS})
endif()
```

### Example 4: Sanitizer CI Workflow Job
```yaml
# Source: Pattern from cmake-linux.yml + sanitizer best practices
sanitizer-sweep:
  runs-on: ubuntu-24.04
  strategy:
    matrix:
      include:
        - name: asan-ubsan
          sanitizer: "address,undefined"
          env_opts: "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
        - name: tsan
          sanitizer: "thread"
          env_opts: "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1"
  steps:
    - uses: actions/checkout@v6
    - name: Install Clang
      run: wget -qO - https://apt.llvm.org/llvm.sh | sudo bash -s -- 19 all
    - name: Configure
      run: |
        cmake -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_COMPILER=clang++-19 \
          -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
          -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind" \
          -DBTOP_SANITIZER="${{ matrix.sanitizer }}"
    - name: Build
      run: cmake --build build
    - name: Run tests
      env:
        ${{ matrix.env_opts }}
      run: ctest --test-dir build --output-on-failure
    - name: Run benchmark workload
      env:
        ${{ matrix.env_opts }}
      run: ./build/btop --benchmark 10
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Clang frontend PGO (`-fprofile-instr-generate`) | Clang IR PGO (`-fprofile-generate`) | LLVM 9+ (2019) | IR PGO preserves profiles better through optimization passes, supports value profiling. Recommended for PGO (frontend PGO now recommended only for code coverage). |
| Separate ASan and UBSan builds | Combined ASan+UBSan (`-fsanitize=address,undefined`) | GCC 5+ / Clang 4+ | Reduces build matrix from 3 to 2. ASan and UBSan are fully compatible. |
| Manual LD_PRELOAD timing | hyperfine statistical benchmarking | hyperfine 1.0 (2018+) | Provides warmup, outlier detection, statistical confidence, and export to JSON/markdown. |
| mimalloc v1.x / v2.0.x | mimalloc v3.2.8 | June 2025 | v3.x improved macOS performance, better ARM64 support, assumes armv8.1-a for fast atomics. |

**Deprecated/outdated:**
- `-fprofile-instr-generate` for PGO: Now recommended only for code coverage, not optimization. Use `-fprofile-generate` instead.
- Separate profile directories for GCC PGO: GCC now defaults to writing `.gcda` files alongside the `.gcno` files in the build tree.

## Open Questions

1. **Exact profile path handling for GCC PGO**
   - What we know: GCC writes `.gcda` files to the same directory as `.gcno` files (the object file directory). `-fprofile-use` can accept a directory path.
   - What's unclear: Whether `-fprofile-use=<directory>` with a different build directory correctly maps profiles to source files. May need `-fprofile-dir` to control output location.
   - Recommendation: Test on Linux CI. If GCC profile path resolution is problematic, document the workaround (using same build directory for generate and use phases, or `-fprofile-dir`).

2. **TSan compatibility with btop's custom atomic_lock**
   - What we know: btop uses relaxed memory ordering (`std::memory_order_relaxed`) for its atomic synchronization. TSan may report false positives for relaxed atomics since they don't establish happens-before relationships in the C++ memory model.
   - What's unclear: Whether TSan will flag btop's atomic_lock pattern as a data race, and whether those reports would be true races or false positives.
   - Recommendation: Run TSan first without suppressions. If false positives appear, add a `tsan.supp` suppression file. If true races appear, fix them.

3. **mimalloc performance on btop's allocation profile**
   - What we know: btop's allocation profile after Phases 2-5 is dominated by SSO-sized strings, fmt::format_to back-inserter buffers, and occasional larger allocations (process list vectors). mimalloc excels at small allocations but may not help with SSO strings (which don't hit the allocator at all).
   - What's unclear: Whether there are enough remaining heap allocations after all optimizations for mimalloc to make a measurable difference.
   - Recommendation: The benchmark will answer this. A "no significant difference" result is a valid and useful finding -- it confirms the Phases 2-5 optimizations successfully reduced allocation overhead.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 + nanobench v4.3.11 |
| Config file | tests/CMakeLists.txt, benchmarks/CMakeLists.txt |
| Quick run command | `ctest --test-dir build --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure && ./build/btop --benchmark 20` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BILD-01 | PGO CMake workflow exists and produces measurable speedup | integration (manual) | `cmake -B build-pgo-gen -DBTOP_PGO_GENERATE=ON && cmake --build build-pgo-gen && ./build-pgo-gen/btop --benchmark 50 && llvm-profdata merge ... && cmake -B build-pgo-use -DBTOP_PGO_USE=btop.profdata && cmake --build build-pgo-use && hyperfine ...` | No -- Wave 0 (CMake options + shell script) |
| BILD-02a | ASan+UBSan pass cleanly | smoke | `cmake -B build-asan -DBTOP_SANITIZER="address,undefined" && cmake --build build-asan && ctest --test-dir build-asan && ./build-asan/btop --benchmark 10` | No -- Wave 0 (CMake option) |
| BILD-02b | TSan passes cleanly | smoke | `cmake -B build-tsan -DBTOP_SANITIZER="thread" && cmake --build build-tsan && ctest --test-dir build-tsan && ./build-tsan/btop --benchmark 10` | No -- Wave 0 (CMake option) |
| BILD-03 | mimalloc benchmarked as drop-in replacement | integration (manual) | `hyperfine './btop --benchmark 50' 'LD_PRELOAD=... ./btop --benchmark 50'` | No -- Wave 0 (shell script) |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build --output-on-failure`
- **Per wave merge:** Full test suite + sanitizer builds + benchmark comparison
- **Phase gate:** All sanitizers clean, PGO measured, mimalloc documented

### Wave 0 Gaps
- [ ] CMake PGO options (BTOP_PGO_GENERATE, BTOP_PGO_USE) -- covers BILD-01
- [ ] CMake sanitizer option (BTOP_SANITIZER) -- covers BILD-02
- [ ] hyperfine installation verification -- covers BILD-01, BILD-03
- [ ] CI sanitizer workflow job -- covers BILD-02 (regression prevention)

## Sources

### Primary (HIGH confidence)
- [LLVM PGO Documentation](https://llvm.org/docs/HowToBuildWithPGO.html) - PGO build workflow, IR PGO flags, llvm-profdata usage
- [Clang ThreadSanitizer Docs](https://clang.llvm.org/docs/ThreadSanitizer.html) - TSan usage, limitations, platform support
- [Clang AddressSanitizer Docs](https://clang.llvm.org/docs/AddressSanitizer.html) - ASan platform support, flags
- [mimalloc GitHub](https://github.com/microsoft/mimalloc) - LD_PRELOAD usage, version info, ARM64 support
- [hyperfine GitHub](https://github.com/sharkdp/hyperfine) - Installation, usage, comparison features
- [LLVM Discussion: IR PGO vs Frontend PGO](https://discourse.llvm.org/t/status-of-ir-vs-frontend-pgo-fprofile-generate-vs-fprofile-instr-generate/58323) - IR PGO recommended for optimization, frontend PGO for coverage

### Secondary (MEDIUM confidence)
- [JetBrains: LTO, PGO, and Unity Builds](https://blog.jetbrains.com/clion/2022/05/testing-3-approaches-performance-cpp_apps/) - PGO+LTO combined results
- [Johnny's Software Lab: PGO](https://johnnysswlab.com/tune-your-programs-speed-with-profile-guided-optimizations/) - GCC PGO flags, two-phase workflow
- [stablecoder.ca: Sanitizer Build Types](http://www.stablecoder.ca/2018/02/01/analyzer-build-types.html) - CMake sanitizer integration patterns
- [mimalloc Issue #676](https://github.com/microsoft/mimalloc/issues/676) - macOS performance regression resolved in v2.1.4+
- [mimalloc Benchmarks](https://microsoft.github.io/mimalloc/bench.html) - Performance characteristics by allocation size

### Tertiary (LOW confidence)
- macOS Apple Silicon sanitizer stability: Based on community reports, not official Apple documentation. System Clang generally works better than Homebrew Clang for sanitizers on macOS. Linux CI (ubuntu-24.04) is the reliable target for sanitizer sweeps.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - PGO flags, sanitizer flags, and mimalloc LD_PRELOAD are well-documented by compiler vendors and verified across multiple authoritative sources
- Architecture: HIGH - Two-phase PGO build, separate sanitizer builds, and LD_PRELOAD allocator swap are established patterns with extensive documentation
- Pitfalls: HIGH - TSan/atomic_lock interaction and macOS sanitizer issues are documented in official issue trackers; PGO+LTO flag propagation is documented in LLVM docs

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (30 days -- stable domain, well-established tooling)
