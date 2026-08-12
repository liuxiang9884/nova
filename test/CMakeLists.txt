find_package(GTest CONFIG REQUIRED)

add_executable(nova_log_backend_options_test
        nova/utils/log_backend_options_test.cpp)
target_include_directories(nova_log_backend_options_test
        PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(nova_log_backend_options_test
        PRIVATE nova GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(nova_log_backend_options_test)
