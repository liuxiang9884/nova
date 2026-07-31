include(FetchContent)

function(nova_find_cpp_yyjson)
    find_package(cpp_yyjson CONFIG QUIET)
    if (cpp_yyjson_FOUND)
        message(STATUS "Found cpp_yyjson: ${cpp_yyjson_DIR}")
        return()
    endif ()

    message(STATUS "cpp_yyjson package not found; using FetchContent fallback")
    FetchContent_Declare(
            cpp-yyjson
            GIT_REPOSITORY https://github.com/yosh-matsuda/cpp-yyjson.git
            GIT_TAG de04a517b76c302bdfcc0ff9f96d98908239af21
            GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(cpp-yyjson)
endfunction()
