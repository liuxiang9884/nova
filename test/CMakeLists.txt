enable_testing()

find_package(GTest CONFIG REQUIRED)

set(BINARY ${CMAKE_PROJECT_NAME}_tst)

file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.hpp *.cpp *.cc)

set(SOURCES ${TEST_SOURCES} ${SOURCES})
message(STATUS "SOURCES: ${SOURCES}")

add_executable(${BINARY} ${SOURCES})
target_include_directories(${BINARY} PRIVATE 
        ${VCPKG_INCLUDE}
        ${NOVA_INCLUDE}
        ${HEADERS}
        ${SOURCES})

message(STATUS "bin: ${BINARY}")


target_link_libraries(${BINARY} PUBLIC 
    nova
    GTest::gtest 
    GTest::gtest_main 
    GTest::gmock 
    GTest::gmock_main
)

add_test(NAME ${BINARY} COMMAND ${BINARY})