# tomlplusplus_demo
add_executable(tomlplusplus_demo tomlplusplus_demo.cpp)
target_include_directories(tomlplusplus_demo PRIVATE ${VCPKG_INCLUDE}
        ${NOVA_INCLUDE})
target_link_libraries(tomlplusplus_demo PRIVATE PkgConfig::tomlplusplus
        fmt::fmt-header-only nova)

# CLI11_demo
add_executable(CLI11_demo CLI11_demo.cpp)
target_include_directories(CLI11_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(CLI11_demo PRIVATE CLI11::CLI11 fmt::fmt-header-only nova)

# log_demo
add_executable(log_demo log_demo.cpp)
target_include_directories(log_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(log_demo PRIVATE ${THIRD_PARTY_LIBS} nova)

# seqlock_demo
add_executable(seqlock_demo seqlock_demo.cpp)
target_include_directories(seqlock_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(seqlock_demo PRIVATE fmt::fmt-header-only nova)

# spsc_queue_demo
add_executable(spsc_queue_demo spsc_queue_demo.cpp)
target_include_directories(spsc_queue_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(spsc_queue_demo PRIVATE fmt::fmt-header-only nova)

# sp_broadcast_queue_demo
add_executable(sp_broadcast_queue_demo sp_broadcast_queue_demo.cpp)
target_include_directories(sp_broadcast_queue_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(sp_broadcast_queue_demo PRIVATE fmt::fmt-header-only nova)

# binary_file_demo
add_executable(binary_file_demo binary_file_demo.cpp)
target_include_directories(binary_file_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(binary_file_demo PRIVATE fmt::fmt-header-only nova)

# mapped_binary_file_demo
add_executable(mapped_binary_file_demo mapped_binary_file_demo.cpp)
target_include_directories(mapped_binary_file_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(mapped_binary_file_demo PRIVATE fmt::fmt-header-only nova)

# datetime_demo
add_executable(datetime_demo datetime_demo.cpp)
target_include_directories(datetime_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(datetime_demo PRIVATE fmt::fmt-header-only nova)

# ring_buffer_demo
add_executable(ring_buffer_demo ring_buffer_demo.cpp)
target_include_directories(ring_buffer_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(ring_buffer_demo PRIVATE fmt::fmt-header-only nova)

# ring_pool_demo
add_executable(ring_pool_demo ring_pool_demo.cpp)
target_include_directories(ring_pool_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(ring_pool_demo PRIVATE fmt::fmt-header-only nova)

# flat_hash_map_demo
add_executable(flat_hash_map_demo flat_hash_map_demo.cpp)
target_include_directories(flat_hash_map_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(flat_hash_map_demo PRIVATE fmt::fmt-header-only nova)

# shm_allocator_demo
add_executable(shm_allocator_demo shm_allocator_demo.cpp)
target_include_directories(shm_allocator_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(shm_allocator_demo PRIVATE fmt::fmt-header-only nova)

# fixed_string_demo
add_executable(fixed_string_demo fixed_string_demo.cpp)
target_include_directories(fixed_string_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(fixed_string_demo PRIVATE fmt::fmt-header-only nova)