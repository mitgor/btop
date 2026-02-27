#!/usr/bin/env bash
# PGO (Profile-Guided Optimization) build script for btop
# Usage: ./scripts/pgo-build.sh [Release|RelWithDebInfo]
#
# This script automates the full PGO workflow:
# 1. Build instrumented binary (profile generation)
# 2. Run training workload (btop --benchmark 50)
# 3. Merge profiles (Clang: llvm-profdata; GCC: automatic)
# 4. Rebuild with profile data (profile use)
#
# The final optimized binary is in build-pgo-use/btop
set -euo pipefail

BUILD_TYPE="${1:-Release}"
CXX_COMPILER="${CXX:-c++}"

echo "=== PGO Build ==="
echo "Build type: $BUILD_TYPE"
echo "Compiler: $CXX_COMPILER"

# Step 1: Build instrumented binary
echo ""
echo "--- Step 1: Building instrumented binary ---"
cmake -B build-pgo-gen -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
  -DBTOP_PGO_GENERATE=ON \
  -DBTOP_LTO=ON
cmake --build build-pgo-gen

# Step 2: Run training workload
echo ""
echo "--- Step 2: Running training workload (btop --benchmark 50) ---"
./build-pgo-gen/btop --benchmark 50

# Step 3: Merge profiles
echo ""
echo "--- Step 3: Merging profiles ---"
IS_CLANG=false
if "$CXX_COMPILER" --version 2>&1 | grep -qi clang; then
  IS_CLANG=true
fi

if [ "$IS_CLANG" = true ]; then
  # Clang: merge .profraw files into .profdata
  PROFRAW_FILES=$(find build-pgo-gen -name "*.profraw" 2>/dev/null || find . -name "default_*.profraw" 2>/dev/null)
  if [ -z "$PROFRAW_FILES" ]; then
    # Clang may write to current directory instead of build directory
    PROFRAW_FILES=$(find . -maxdepth 1 -name "*.profraw" 2>/dev/null)
  fi
  if [ -z "$PROFRAW_FILES" ]; then
    echo "ERROR: No .profraw files found. Profile generation may have failed."
    exit 1
  fi
  # shellcheck disable=SC2086
  llvm-profdata merge -output=btop.profdata $PROFRAW_FILES
  PROFILE_PATH="$(pwd)/btop.profdata"
  echo "Clang profile merged: $PROFILE_PATH"
else
  # GCC: .gcda files are written alongside .gcno files in the build directory
  PROFILE_PATH="$(pwd)/build-pgo-gen"
  GCDA_COUNT=$(find "$PROFILE_PATH" -name "*.gcda" 2>/dev/null | wc -l)
  if [ "$GCDA_COUNT" -eq 0 ]; then
    echo "ERROR: No .gcda files found in $PROFILE_PATH. Profile generation may have failed."
    exit 1
  fi
  echo "GCC profiles: $GCDA_COUNT .gcda files in $PROFILE_PATH"
fi

# Step 4: Rebuild with profile data
echo ""
echo "--- Step 4: Building optimized binary with PGO ---"
cmake -B build-pgo-use -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
  -DBTOP_PGO_USE="$PROFILE_PATH" \
  -DBTOP_LTO=ON
cmake --build build-pgo-use

echo ""
echo "=== PGO Build Complete ==="
echo "Optimized binary: build-pgo-use/btop"
echo ""
echo "To benchmark PGO improvement, install hyperfine and run:"
echo "  hyperfine --warmup 3 --min-runs 10 \\"
echo "    './build-pgo-gen/btop --benchmark 50' \\"
echo "    './build-pgo-use/btop --benchmark 50'"
