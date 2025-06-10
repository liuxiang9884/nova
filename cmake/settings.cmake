#vcpkg
set(VCPKG_ROOT "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
if (APPLE)
    set(VCPKG_INCLUDE "$ENV{HOME}/vcpkg/installed/arm64-osx/include")
    set(CMAKE_OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk"
            CACHE STRING "SDK_PATH" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.5")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
elseif (WIN32)
    set(VCPKG_INCLUDE "$ENV{HOME}/vcpkg/installed/x64-windows/include")
elseif (UNIX)
    set(VCPKG_INCLUDE "$ENV{HOME}/vcpkg/installed/x64-linux/include")
endif()
set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "VCPKG toolchain file")

message(STATUS "VCPKG_ROOT: ${VCPKG_ROOT}")
message(STATUS "VCPKG_INCLUDE: ${VCPKG_INCLUDE}")
message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
#nova
set(NOVA_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")