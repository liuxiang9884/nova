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
# magic enum
find_package(magic_enum CONFIG REQUIRED)
# yyjson
find_package(yyjson CONFIG REQUIRED)
# nameof
find_package(nameof CONFIG REQUIRED)
# drogon
find_package(Drogon CONFIG REQUIRED)

set(THIRD_PARTY_LIBS
        CLI11::CLI11
        PkgConfig::tomlplusplus
        quill::quill
        magic_enum::magic_enum
        fmt::fmt-header-only
        yyjson::yyjson
        nameof::nameof
        Drogon::Drogon
)