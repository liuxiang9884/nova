cmake_minimum_required(VERSION 3.30)

include(cmake/settings.cmake)

project(nova VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
include(cmake/package.cmake)

if (MSVC)
    add_compile_options(/W4)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

file(GLOB_RECURSE HEADERS
        include/*.h
        include/*.hpp
)

file(GLOB_RECURSE SOURCES
        src/*.c
        src/*.cc
        src/*.cpp
)

message(STATUS "Headers: ${HEADERS}")
message(STATUS "Sources: ${SOURCES}")

add_library(nova ${HEADERS} ${SOURCES})
target_include_directories(nova PUBLIC ${NOVA_INCLUDE} ${VCPKG_INCLUDE})
target_compile_options(nova PRIVATE -Wno-gnu-zero-variadic-macro-arguments)

add_executable(nova_demo main.cpp)
target_link_libraries(nova_demo PRIVATE nova)

if (PROJECT_IS_TOP_LEVEL)
    add_subdirectory(examples)
endif ()




