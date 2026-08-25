find_package(CLI11 CONFIG REQUIRED)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/packages.cmake")

add_executable(tomlplusplus_demo tomlplusplus_demo.cpp)
target_link_libraries(tomlplusplus_demo PRIVATE tomlplusplus::tomlplusplus
        fmt::fmt-header-only nova::nova)

add_executable(CLI11_demo CLI11_demo.cpp)
target_link_libraries(CLI11_demo PRIVATE CLI11::CLI11 fmt::fmt-header-only
        nova::nova)

add_executable(log_demo log_demo.cpp)
target_link_libraries(log_demo PRIVATE CLI11::CLI11 fmt::fmt-header-only
        nova::nova)

add_executable(seqlock_demo seqlock_demo.cpp)
target_link_libraries(seqlock_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(spsc_queue_demo spsc_queue_demo.cpp)
target_link_libraries(spsc_queue_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(sp_broadcast_queue_demo sp_broadcast_queue_demo.cpp)
target_link_libraries(sp_broadcast_queue_demo PRIVATE fmt::fmt-header-only
        nova::nova)

add_executable(binary_file_demo binary_file_demo.cpp)
target_link_libraries(binary_file_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(mapped_binary_file_demo mapped_binary_file_demo.cpp)
target_link_libraries(mapped_binary_file_demo PRIVATE fmt::fmt-header-only
        nova::nova)

add_executable(datetime_demo datetime_demo.cpp)
target_link_libraries(datetime_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(ring_buffer_demo ring_buffer_demo.cpp)
target_link_libraries(ring_buffer_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(ring_pool_demo ring_pool_demo.cpp)
target_link_libraries(ring_pool_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(flat_hash_map_demo flat_hash_map_demo.cpp)
target_link_libraries(flat_hash_map_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(shm_allocator_demo shm_allocator_demo.cpp)
target_link_libraries(shm_allocator_demo PRIVATE fmt::fmt nova::nova)

add_executable(shm_allocator_test shm_allocator_test.cpp)
target_link_libraries(shm_allocator_test PRIVATE fmt::fmt nova::nova)

add_executable(fixed_string_demo fixed_string_demo.cpp)
target_link_libraries(fixed_string_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(fixed_array_demo fixed_array_demo.cpp)
target_link_libraries(fixed_array_demo PRIVATE fmt::fmt-header-only nova::nova)

add_executable(broadcast_queue_demo broadcast_queue_demo.cpp)
target_link_libraries(broadcast_queue_demo PRIVATE fmt::fmt-header-only
        nova::nova)

nova_find_cpp_yyjson()
add_executable(yyjson_demo yyjson_demo.cpp)
target_link_libraries(yyjson_demo PRIVATE fmt::fmt-header-only nova::nova
        cpp_yyjson::cpp_yyjson)
