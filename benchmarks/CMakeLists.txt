# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
FetchContent_Declare(
  nanobench
  GIT_REPOSITORY https://github.com/martinus/nanobench.git
  GIT_TAG v4.3.11
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nanobench)

add_executable(btop_bench_strings bench_string_utils.cpp)
target_include_directories(btop_bench_strings PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(btop_bench_strings PRIVATE libbtop nanobench)

add_executable(btop_bench_draw bench_draw.cpp)
target_include_directories(btop_bench_draw PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(btop_bench_draw PRIVATE libbtop nanobench)

add_executable(btop_bench_ds bench_data_structures.cpp)
target_include_directories(btop_bench_ds PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(btop_bench_ds PRIVATE libbtop nanobench)

if(LINUX)
    add_executable(btop_bench_proc bench_proc_collect.cpp)
    target_include_directories(btop_bench_proc PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(btop_bench_proc PRIVATE libbtop nanobench)
endif()
