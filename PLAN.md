# Plan: cpp-yyjson local vcpkg port for nova
_Locked via grill — by Codex + liuxiang_

## Goal

让 `nova` 的 top-level example `yyjson_demo` 优先使用 vcpkg 提供的 `cpp-yyjson`，避免 CMake configure 阶段通过 `FetchContent` 拉取 `cpp-yyjson` 及其 recursive submodules 导致卡在 GitHub；同时不要让嵌入式消费者（例如 `orion` 通过 `FetchContent` 引入 `nova`）承担 `cpp-yyjson` 这个仅 examples 使用的依赖。未安装本地 port 的环境仍保留 pinned FetchContent fallback。

## Approach

1. 在 `nova` 仓库中新增 `vcpkg-overlay-ports/cpp-yyjson`，实现一个本地 overlay port。
2. overlay port 固定到 `cpp-yyjson` 当前已验证源码 commit `de04a517b76c302bdfcc0ff9f96d98908239af21`，版本按 upstream CMake 项目使用 `0.8.0`，并记录 SHA512。
3. port 依赖 vcpkg 官方 `yyjson`，host dependencies 显式声明 `vcpkg-cmake` 和 `vcpkg-cmake-config`。
4. port 使用 upstream CMake 安装 INTERFACE target `cpp_yyjson::cpp_yyjson` 与 headers；`vcpkg_cmake_config_fixup(PACKAGE_NAME cpp_yyjson CONFIG_PATH lib/cmake/cpp_yyjson)` 显式修复 port name `cpp-yyjson` 与 package name `cpp_yyjson` 的差异。
5. port 作为 header-only package 设置 `set(VCPKG_BUILD_TYPE release)`，避免 two-config install 产生重复的 `debug/include`；同时显式关闭 upstream tests 和 benchmarks：`CPPYYJSON_BUILD_TEST=OFF`、`CPPYYJSON_BUILD_BENCH=OFF`；安装 `LICENSE` 到 copyright。
6. 修改 `cmake/packages.cmake`：移除全局 `cpp-yyjson` FetchContent，使 `nova` library 和嵌入式消费者不再解析该依赖。
7. 在 `examples/CMakeLists.txt` 的 `yyjson_demo` 附近做局部依赖解析：先 `find_package(cpp_yyjson CONFIG QUIET)`；找不到时用 pinned HTTPS FetchContent fallback，`GIT_TAG de04a517b76c302bdfcc0ff9f96d98908239af21`，`GIT_SUBMODULES ""`，避免 benchmark submodule。
8. 修改 `CMakeLists.txt` 和 `examples/CMakeLists.txt`：从 `nova` target 和 `yyjson_demo` include directories 移除 `CPP_YYJSON_INCLUDE`，`yyjson_demo` 直接 `target_link_libraries(... cpp_yyjson::cpp_yyjson)`，利用 target 的 transitive include、C++20 和 `yyjson` linkage。
9. 更新 `README.md`：classic mode 下只在显式安装命令中使用 overlay：`vcpkg install cpp-yyjson --overlay-ports=<repo>/vcpkg-overlay-ports --triplet x64-linux`；安装完成后的 CMake 配置只说明 toolchain 和 selected triplet，不再要求 overlay path；记录当前本机 vcpkg commit `96d5fb3de135b86d7222c53f2352ca92827a156b` 和 `x64-linux` triplet。
10. 验证 `vcpkg install cpp-yyjson --overlay-ports=/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/vcpkg-overlay-ports --triplet x64-linux` 成功。
11. 用 fresh build directory 验证 `nova` top-level CMake configure：开启 `--debug-find-pkg=cpp_yyjson`，确认 `cpp_yyjson_DIR` 指向 `/home/liuxiang/vcpkg`，并确认 build tree 中不存在 `_deps/cpp-yyjson-src`。
12. 构建并运行 `yyjson_demo`，证明 header-only port、CMake target、transitive `yyjson` linkage 和 C++20 feature 实际可用。
13. 用 fresh build directory 验证 `orion` CMake configure 使用 `/home/liuxiang/dev/nova-cpp-yyjson-vcpkg` 作为 `FETCHCONTENT_SOURCE_DIR_NOVA` 时不解析 `cpp_yyjson`，并确认不会生成 `_deps/cpp-yyjson-src`。
14. 做一个 focused embedded consumer build，例如构建 `orion` 的 `orion` library target 或后续需要的转换工具 target，确认嵌入式 `nova` 不再依赖 `cpp_yyjson`。
15. 另用 fresh build directory 加 `-DCMAKE_DISABLE_FIND_PACKAGE_cpp_yyjson=ON` 强制 smoke fallback branch，确认 fallback 使用 pinned HTTPS 且不初始化 submodules；如果环境限制导致无法实际联网，只记录命令和已验证的 CMake 配置路径，不声称 fallback 已完全验证。

## Key decisions & tradeoffs

- 使用 overlay port 而不是直接修改 `/home/liuxiang/vcpkg/ports`，换取可提交、可审查、可迁移的本地 dependency definition。
- `cpp_yyjson` 只在 top-level examples 中解析，不进入 `nova` library 的公共 include/link contract，避免污染 `orion` 等嵌入式消费者。
- `yyjson_demo` 链接 `cpp_yyjson::cpp_yyjson` target，而不是手工传 include path 和 `yyjson::yyjson`，避免丢失 transitive usage requirements。
- 固定 `cpp-yyjson` commit，不继续跟随 upstream `main`，换取该依赖本身的可复现性；完整环境复现范围仅限文档记录的 vcpkg commit、triplet 和 installed feature set。
- fallback 使用 HTTPS、固定 commit 且禁用 submodules，避免 SSH credential 依赖和 benchmark submodule 卡住。
- fallback 测试必须显式禁用 `find_package(cpp_yyjson)`，否则已安装的 classic-mode package 会遮蔽 fallback 分支。
- 当前工作在隔离 worktree `/home/liuxiang/dev/nova-cpp-yyjson-vcpkg` 和分支 `build/cpp-yyjson-vcpkg` 上完成，不直接修改 `/home/liuxiang/dev/nova` 的主工作区。
- 本阶段不跑完整 debug/release 编译，但会做 focused build/run：`yyjson_demo` 和一个 `orion` embedded consumer target。

## Risks / open questions

- overlay port 的 `SHA512` 需要从 vcpkg 下载实际 source archive 后确认；不能留空或用未验证 hash。
- `cpp-yyjson` upstream port install 可能默认构建 tests 或 benchmarks；port 需要显式关闭不必要目标。
- header-only port 需要避免安装 duplicate debug headers，优先用 `set(VCPKG_BUILD_TYPE release)`。
- `orion` 的完整构建还可能遇到与 `cpp-yyjson` 无关的 FetchContent、vcpkg 或平台依赖问题；本计划只解决 `cpp-yyjson` FetchContent 卡住问题。
- vcpkg classic mode 下安装 overlay port 需要用户或构建命令显式传入 `--overlay-ports`，否则仍找不到 `cpp-yyjson`。
- fallback branch 如果需要联网验证，可能仍受 GitHub 网络影响；即使失败，也应清楚区分“fallback 网络不可验证”和“本机 vcpkg path 可用”。

## Out of scope

- 不把 `cpp-yyjson` 提交到 vcpkg 官方 registry。
- 不引入 `vcpkg.json` manifest 模式迁移。
- 不移除 `FetchContent` fallback。
- 不修改 `orion` 源码中的 `FetchContent_Declare(nova)`。
- 不默认执行 `nova` 或 `orion` 的完整 debug/release 编译。
