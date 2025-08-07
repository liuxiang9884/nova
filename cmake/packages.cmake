include(FetchContent)

FetchContent_Declare(
        cpp-yyjson
        GIT_REPOSITORY git@github.com:yosh-matsuda/cpp-yyjson.git
        GIT_TAG main
)

FetchContent_MakeAvailable(cpp-yyjson)
set(CPP_YYJSON_INCLUDE ${cpp-yyjson_SOURCE_DIR}/include)
set(CPP_YYJSON_INCLUDE ${CPP_YYJSON_INCLUDE} CACHE STRING "cpp-yyjson include directory" FORCE)
message(STATUS "CPP_YYJSON_INCLUDE: " ${CPP_YYJSON_INCLUDE})