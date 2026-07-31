vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO yosh-matsuda/cpp-yyjson
    REF de04a517b76c302bdfcc0ff9f96d98908239af21
    SHA512 bef05fcf54e215f9745661c846c3b0fa3c058440d26cfcb253574d7929ef14d1d07cf8bfd841f7ddbd43e91f959f0621075d37a981de6eb33b831e12d0327ec1
    HEAD_REF main
)

set(VCPKG_BUILD_TYPE release)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCPPYYJSON_BUILD_TEST=OFF
        -DCPPYYJSON_BUILD_BENCH=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME cpp_yyjson
    CONFIG_PATH lib/cmake/cpp_yyjson
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
