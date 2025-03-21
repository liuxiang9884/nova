cmake_minimum_required(VERSION 3.30)

include(cmake/settings.cmake)

project(nova VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

if (MSVC)
    add_compile_options(/W4)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

# external library installed by vcpkg
# fmtlib
find_package(fmt CONFIG REQUIRED)
# quill
find_package(quill CONFIG REQUIRED)
# tomlplusplus
find_package(PkgConfig REQUIRED)
pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET tomlplusplus)
# CLI11
find_package(CLI11 CONFIG REQUIRED)
#target_link_libraries(main PRIVATE CLI11::CLI11)
add_executable(nova main.cpp
        include/nova/utils/format.h)

add_subdirectory(examples)




