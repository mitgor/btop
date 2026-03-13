# SPDX-License-Identifier: Apache-2.0
#
# CMake configuration for btop
#

cmake_minimum_required(VERSION 3.25)

# Disable in-source builds since they would override the Makefile
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed")
endif()

project("btop"
  DESCRIPTION "A monitor of resources"
  HOMEPAGE_URL "https://github.com/aristocratos/btop"
  LANGUAGES CXX
)

include(CheckIPOSupported)
include(CMakeDependentOption)

# Make our Find<Package>.cmake files available
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BTOP_STATIC "Link btop statically" OFF)
option(BTOP_GPU "Enable GPU support" ON)
cmake_dependent_option(BTOP_RSMI_STATIC "Link statically to ROCm SMI" OFF "BTOP_GPU" OFF)

# Enable LTO in release builds by default
if(CMAKE_BUILD_TYPE MATCHES "Rel(ease|WithDebInfo)")
  option(BTOP_LTO "Enable LTO" ON)
else()
  option(BTOP_LTO "Enable LTO" OFF)
endif()

# Profile-Guided Optimization
option(BTOP_PGO_GENERATE "Build with PGO instrumentation (generates profile data)" OFF)
set(BTOP_PGO_USE "" CACHE FILEPATH "Path to PGO profile data for optimized build")

if(BTOP_PGO_GENERATE AND BTOP_PGO_USE)
  message(FATAL_ERROR "Cannot set both BTOP_PGO_GENERATE and BTOP_PGO_USE")
endif()

# Sanitizer support
set(BTOP_SANITIZER "" CACHE STRING "Enable sanitizer (address, thread, undefined, or combinations like address,undefined)")

if(BTOP_PGO_GENERATE AND BTOP_SANITIZER)
  message(FATAL_ERROR "Cannot combine PGO instrumentation with sanitizers")
endif()
if(BTOP_PGO_USE AND BTOP_SANITIZER)
  message(FATAL_ERROR "Cannot combine PGO optimization with sanitizers")
endif()

option(BTOP_BENCHMARKS "Build performance benchmarks" OFF)

# Native CPU optimization for local builds
option(BTOP_NATIVE "Optimize for the build machine's CPU (-march=native)" OFF)

# Precompiled headers for faster builds
option(BTOP_PCH "Use precompiled headers for faster clean builds" OFF)

if(BTOP_STATIC AND NOT APPLE)
  # Set this before calling find_package
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

add_executable(btop src/main.cpp)
add_library(libbtop OBJECT
  src/btop.cpp
  src/btop_cli.cpp
  src/btop_config.cpp
  src/btop_draw.cpp
  src/btop_input.cpp
  src/btop_log.cpp
  src/btop_menu.cpp
  src/btop_shared.cpp
  src/btop_theme.cpp
  src/btop_tools.cpp
)

target_link_libraries(btop libbtop)

if(BTOP_LTO)
  check_ipo_supported()
  set_target_properties(btop PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  set_target_properties(libbtop PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# Apply PGO flags to both targets
if(BTOP_PGO_GENERATE)
  message(STATUS "PGO: Instrumentation build (profile generation)")
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

# Apply sanitizer flags
if(BTOP_SANITIZER)
  message(STATUS "Sanitizer enabled: ${BTOP_SANITIZER}")
  target_compile_options(libbtop PUBLIC
    -fsanitize=${BTOP_SANITIZER}
    -fno-omit-frame-pointer
    -g
  )
  target_link_options(libbtop PUBLIC -fsanitize=${BTOP_SANITIZER})
  target_compile_options(btop PUBLIC
    -fsanitize=${BTOP_SANITIZER}
    -fno-omit-frame-pointer
    -g
  )
  target_link_options(btop PUBLIC -fsanitize=${BTOP_SANITIZER})
endif()

if(APPLE)
  target_sources(libbtop PRIVATE src/osx/btop_collect.cpp src/osx/sensors.cpp src/osx/smc.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "MidnightBSD")
  target_sources(libbtop PRIVATE src/freebsd/btop_collect.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  target_sources(libbtop PRIVATE src/openbsd/btop_collect.cpp src/openbsd/sysctlbyname.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  target_sources(libbtop PRIVATE src/netbsd/btop_collect.cpp)
elseif(LINUX)
  target_sources(libbtop PRIVATE src/linux/btop_collect.cpp)
  if(BTOP_GPU)
    add_subdirectory(src/linux/intel_gpu_top)
  endif()
else()
  message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

# Generate build info
execute_process(
  COMMAND "git" "rev-parse" "--short" "HEAD"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
set(CONFIGURE_COMMAND
    "cmake -DBTOP_STATIC=${BTOP_STATIC} -DBTOP_GPU=${BTOP_GPU}"
)
get_filename_component(CXX_COMPILER_BASENAME "${CMAKE_CXX_COMPILER}" NAME)
set(COMPILER "${CXX_COMPILER_BASENAME}")
set(COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY IMMEDIATE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(CheckCXXSourceCompiles)
include(CheckCXXSymbolExists)

check_cxx_source_compiles(
  "
  #include <version>
  #if __cpp_lib_optional < 202110L
  #error \"Missing __cpp_lib_optional < 202110L\"
  #endif
  int main() { return 0; }
  "
  HAS_CXX_OPTIONAL_MONADS
)
if(NOT HAS_CXX_OPTIONAL_MONADS)
  message(FATAL_ERROR "The compiler doesn't support std::optional::and_then")
endif()
check_cxx_symbol_exists("__cpp_lib_expected" "version" HAS_CXX_EXPECTED)
if(NOT HAS_CXX_EXPECTED)
  message(FATAL_ERROR "The compiler doesn't support std::expected")
endif()
check_cxx_symbol_exists("__cpp_lib_ranges" "version" HAS_CXX_RANGES)
if(NOT HAS_CXX_RANGES)
  message(FATAL_ERROR "The compiler doesn't support std::ranges")
endif()
check_cxx_symbol_exists("__cpp_lib_ranges_to_container" "version" HAS_CXX_RANGES_TO_CONTAINER)
if(NOT HAS_CXX_RANGES_TO_CONTAINER)
  message(FATAL_ERROR "The compiler doesn't support std::ranges::to")
endif()
check_cxx_symbol_exists("__cpp_lib_string_contains" "version" HAS_CXX_STRING_CONTAINS)
if(NOT HAS_CXX_STRING_CONTAINS)
  message(FATAL_ERROR "The compiler doesn't support std::string::contains")
endif()

target_compile_options(libbtop PUBLIC -Wall -Wextra -Wpedantic)

include(CheckCXXCompilerFlag)

if(NOT APPLE)
  target_compile_options(libbtop PUBLIC -fstack-clash-protection)
endif()
check_cxx_compiler_flag(-fstack-protector HAS_FSTACK_PROTECTOR)
if(HAS_FSTACK_PROTECTOR)
  target_compile_options(libbtop PUBLIC -fstack-protector)
endif()
check_cxx_compiler_flag(-fcf-protection HAS_FCF_PROTECTION)
if(HAS_FCF_PROTECTION)
  target_compile_options(libbtop PUBLIC -fcf-protection)
endif()

# Apply native CPU optimization
if(BTOP_NATIVE)
  check_cxx_compiler_flag(-march=native HAS_MARCH_NATIVE)
  if(HAS_MARCH_NATIVE)
    message(STATUS "Native CPU optimization enabled (-march=native)")
    target_compile_options(libbtop PUBLIC -march=native)
    target_compile_options(btop PUBLIC -march=native)
  else()
    message(WARNING "Compiler does not support -march=native, ignoring BTOP_NATIVE")
    set(BTOP_NATIVE OFF)
  endif()
endif()

# Apply precompiled headers
if(BTOP_PCH)
  target_precompile_headers(libbtop PRIVATE
    <algorithm>
    <array>
    <atomic>
    <chrono>
    <cstdint>
    <filesystem>
    <fmt/format.h>
    <iterator>
    <mutex>
    <optional>
    <ranges>
    <string>
    <string_view>
    <utility>
    <variant>
    <vector>
  )
  message(STATUS "Precompiled headers enabled")
endif()

target_compile_definitions(libbtop PUBLIC
  FMT_HEADER_ONLY
  _FILE_OFFSET_BITS=64
  $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG>
)

target_include_directories(libbtop SYSTEM PUBLIC include)

# Enable pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(libbtop Threads::Threads)

# Enable GPU support
if(LINUX AND BTOP_GPU)
  target_compile_definitions(libbtop PUBLIC GPU_SUPPORT)

  if(BTOP_RSMI_STATIC)
    # ROCm doesn't properly add it's folders to the module path if `CMAKE_MODULE_PATH` is already
    # set
    # We could also manually append ROCm's path here
    set(_CMAKE_MODULE_PATH CMAKE_MODULE_PATH)
    unset(CMAKE_MODULE_PATH)

    # NOTE: This might be problematic in the future if other sub projects depend on this or if
    # btop starts producing libraries
    # Build a static ROCm library
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    add_subdirectory(lib/rocm_smi_lib EXCLUDE_FROM_ALL)

    add_library(ROCm INTERFACE)
    # Export ROCm's properties to a target
    target_compile_definitions(ROCm INTERFACE RSMI_STATIC)
    target_include_directories(ROCm INTERFACE lib/rocm_smi_lib/include)
    target_link_libraries(ROCm INTERFACE rocm_smi64)

    set(CMAKE_MODULE_PATH _CMAKE_MODULE_PATH)

    target_link_libraries(libbtop ROCm)
  endif()
  if(NOT BTOP_STATIC)
    target_link_libraries(libbtop ${CMAKE_DL_LIBS})
  endif()
endif()

if(BTOP_STATIC)
  target_compile_definitions(libbtop PUBLIC STATIC_BUILD)
  target_link_options(libbtop PUBLIC -static LINKER:--fatal-warnings)
endif()

# Enable GPU support on Apple Silicon
if(APPLE AND BTOP_GPU)
  target_compile_definitions(libbtop PUBLIC GPU_SUPPORT)
endif()

# Other platform dependent flags
if(APPLE)
  target_link_libraries(libbtop
    $<LINK_LIBRARY:FRAMEWORK,CoreFoundation> $<LINK_LIBRARY:FRAMEWORK,IOKit>
  )
  if(BTOP_GPU)
    find_library(IOREPORT_LIB IOReport)
    if(IOREPORT_LIB)
      target_link_libraries(libbtop ${IOREPORT_LIB})
    endif()
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "MidnightBSD")
  # Avoid version mismatch for libstdc++ when a specific version of GCC is installed and not the
  # default one since all use the default ones RPATH
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    string(REGEX MATCH "^[0-9]+" GCC_VERSION_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
    set_target_properties(btop PROPERTIES
      INSTALL_RPATH "/usr/local/lib/gcc${GCC_VERSION_MAJOR}"
      BUILD_WITH_INSTALL_RPATH TRUE
    )

    # The gcc compiler wrapper doesn't add '--eh-frame-hdr' on FreeBSD with static builds
    # https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=278551
    if(BTOP_STATIC)
      target_link_options(libbtop PUBLIC LINKER:--eh-frame-hdr)
    endif()
  endif()

  find_package(devstat REQUIRED)
  find_package(kvm REQUIRED)
  target_link_libraries(libbtop devstat::devstat kvm::kvm)
  if(BTOP_STATIC)
    find_package(elf REQUIRED)
    target_link_libraries(libbtop elf::elf)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libbtop PUBLIC -static-libstdc++)
  endif()
  find_package(kvm REQUIRED)
  target_link_libraries(libbtop kvm::kvm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libbtop PUBLIC -static-libstdc++)
  endif()
  find_package(kvm REQUIRED)
  find_package(proplib REQUIRED)
  target_link_libraries(libbtop kvm::kvm proplib::proplib)
endif()

# Check if lowdown is installed
find_program(LOWDOWN_EXECUTABLE lowdown)

if(LOWDOWN_EXECUTABLE)
  # Custom target to compile Markdown to man page using lowdown
  add_custom_command(
    OUTPUT btop.1
    COMMAND lowdown -s -T man -o btop.1 ${CMAKE_CURRENT_SOURCE_DIR}/manpage.md
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/manpage.md
    VERBATIM
  )
  add_custom_target(generate_manpage ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/btop.1)
  # Install the man page
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/btop.1" DESTINATION "share/man/man1")
else()
  message(STATUS "Command 'lowdown' not found: skipping generating man page btop.1")
endif()

install(TARGETS btop RUNTIME)
install(FILES "btop.desktop" DESTINATION "share/applications")
install(FILES "Img/icon.png" DESTINATION "share/icons/hicolor/48x48/apps" RENAME "btop.png")
install(FILES "Img/icon.svg" DESTINATION "share/icons/hicolor/scalable/apps" RENAME "btop.svg")
install(DIRECTORY "themes" DESTINATION "share/btop")

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(BTOP_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
