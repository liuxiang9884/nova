find_package(absl CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)

set(NOVA_SET_AVX2_DEFAULT OFF)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$"
        AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(NOVA_SET_AVX2_DEFAULT ON)
endif ()
option(NOVA_SET_BENCHMARK_AVX2 "Include author's AVX2/POPCNT B-Tree (requires capable CPU)"
        ${NOVA_SET_AVX2_DEFAULT})
if (NOVA_SET_BENCHMARK_AVX2 AND NOT NOVA_SET_AVX2_DEFAULT)
    message(FATAL_ERROR "Author B-Tree requires x86-64 with GCC/Clang and AVX2/POPCNT")
endif ()

add_library(nova_btree_set_adapters INTERFACE)
target_include_directories(nova_btree_set_adapters INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nova_btree_set_adapters INTERFACE absl::btree)
target_compile_definitions(nova_btree_set_adapters INTERFACE
        NOVA_SET_HAS_AVX2=$<BOOL:${NOVA_SET_BENCHMARK_AVX2}>)
if (NOVA_SET_BENCHMARK_AVX2)
    # Apply the same ISA flags to all four containers in this comparison.
    target_compile_options(nova_btree_set_adapters INTERFACE -mavx2 -mpopcnt)
else ()
    message(STATUS "btree_set: author_avx2 omitted; building Abseil, std::set and radix bitmap")
endif ()

add_executable(nova_btree_set_benchmark set_benchmark.cpp)
target_link_libraries(nova_btree_set_benchmark PRIVATE
        nova_btree_set_adapters benchmark::benchmark)

if (BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(nova_btree_set_test set_test.cpp radix_bitmap_test.cpp)
    target_link_libraries(nova_btree_set_test PRIVATE
            nova_btree_set_adapters GTest::gtest_main)
    add_test(NAME nova_btree_set_correctness COMMAND nova_btree_set_test)
endif ()
