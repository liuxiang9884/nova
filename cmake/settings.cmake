#vcpkg
set(VCPKG_ROOT "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_INCLUDE "$ENV{HOME}/vcpkg/installed/arm64-osx/include")
set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT} CACHE STRING "Vcpkg toolchain file")

#nova
set(NOVA_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")