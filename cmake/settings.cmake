# vcpkg
if (DEFINED ENV{VCPKG_ROOT})
    set(NOVA_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
else ()
    set(NOVA_VCPKG_ROOT "$ENV{HOME}/vcpkg")
endif ()

if (APPLE)
    set(CMAKE_OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
            CACHE STRING "SDK_PATH" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.5")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif ()

set(CMAKE_TOOLCHAIN_FILE
        "${NOVA_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "Vcpkg toolchain file")

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

# nova
set(NOVA_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")