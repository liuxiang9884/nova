cmake_minimum_required(VERSION 3.20)

project(nova VERSION 0.1.0 LANGUAGES CXX)

include(cmake/third_party.cmake)

file(GLOB_RECURSE HEADERS
        include/*.h
        include/*.hpp
)

file(GLOB_RECURSE SOURCES
        src/*.c
        src/*.cc
        src/*.cpp
)

add_library(nova ${HEADERS} ${SOURCES})
add_library(nova::nova ALIAS nova)

target_compile_features(nova PUBLIC cxx_std_20)
target_include_directories(nova PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(nova SYSTEM INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(nova
        PUBLIC
        fmt::fmt-header-only
        magic_enum::magic_enum
        quill::quill
        tomlplusplus::tomlplusplus)

if (MSVC)
    target_compile_options(nova PRIVATE /W4)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(nova PRIVATE
            -Wall -Wextra -Wpedantic -Wno-interference-size)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(nova PRIVATE
            -Wall -Wextra -Wpedantic
            -Wno-gnu-zero-variadic-macro-arguments)
else ()
    target_compile_options(nova PRIVATE -Wall -Wextra -Wpedantic)
endif ()

if (PROJECT_IS_TOP_LEVEL)
    add_executable(nova_demo main.cpp)
    target_link_libraries(nova_demo PRIVATE nova::nova)

    include(CTest)
    if (BUILD_TESTING)
        add_subdirectory(test)
    endif ()
    add_subdirectory(examples)
endif ()

