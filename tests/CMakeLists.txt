# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

add_library(libbtop_test)
target_include_directories(libbtop_test PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libbtop_test libbtop GTest::gtest_main)

# test_enum_arrays.cpp depends on Task 2 (GraphSymbolType + graph_symbols migration)
# It will be added back after Task 2 completes the enum-indexed array migration.
add_executable(btop_test tools.cpp test_uncolor.cpp test_ring_buffer.cpp)
target_link_libraries(btop_test libbtop_test)

include(GoogleTest)
gtest_discover_tests(btop_test)
