#tomlplusplus_demo
add_executable(tomlplusplus_demo tomlplusplus_demo.cpp)
target_include_directories(tomlplusplus_demo PRIVATE ${VCPKG_INCLUDE}
        ${NOVA_INCLUDE})
target_link_libraries(tomlplusplus_demo PRIVATE PkgConfig::tomlplusplus)