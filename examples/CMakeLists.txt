#tomlplusplus_demo
add_executable(tomlplusplus_demo tomlplusplus_demo.cpp)
target_include_directories(tomlplusplus_demo PRIVATE ${VCPKG_INCLUDE}
        ${NOVA_INCLUDE})
target_link_libraries(tomlplusplus_demo PRIVATE PkgConfig::tomlplusplus
        fmt::fmt-header-only nova)

#CLI11_demo
add_executable(CLI11_demo CLI11_demo.cpp)
target_include_directories(CLI11_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(CLI11_demo PRIVATE CLI11::CLI11 fmt::fmt-header-only nova)

#log_demo
add_executable(log_demo log_demo.cpp)
target_include_directories(log_demo PRIVATE ${VCPKG_INCLUDE} ${NOVA_INCLUDE})
target_link_libraries(log_demo PRIVATE ${THIRD_PARTY_LIBS} nova)
