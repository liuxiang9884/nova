cmake_minimum_required(VERSION 3.30)

include(cmake/settings.cmake)

project(nova VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
include(cmake/package.cmake)

if (MSVC)
    add_compile_options(/W4)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

add_executable(nova main.cpp
        include/nova/utils/enum.h)

add_subdirectory(examples)




