# nova

nova 是一个使用 C++20 的基础库，提供容器、并发队列、日志、文件访问和共享内存等组件，并包含示例程序与单元测试。

## 构建环境

- 支持 C++20 的 C/C++ 编译器。
- CMake 3.21 或更新版本，以及 Make 或 Ninja。
- Git、pkg-config，以及首次安装依赖时访问 GitHub 的网络环境。
- macOS 需要安装 Xcode Command Line Tools（`xcode-select --install`）。当前 CMake 配置默认使用 Apple Silicon（`arm64`）。

## 依赖库

根目录的 `vcpkg.json` 声明了当前默认构建所需的直接依赖；vcpkg 会自动安装它们及其传递依赖。

| vcpkg 包名 | 用途 |
| --- | --- |
| `cli11` | 命令行参数解析，供示例程序使用 |
| `fmt` | 字符串格式化 |
| `magic-enum` | 枚举反射和转换 |
| `quill` | 异步日志 |
| `tomlplusplus` | TOML 配置解析 |
| `yyjson` | JSON 解析与生成，也是 `cpp-yyjson` 的依赖 |
| `cpp-yyjson` | yyjson 的 C++20 封装，供 `yyjson_demo` 使用 |
| `nameof` | 名称反射，当前 CMake 依赖配置保留此库 |
| `gtest` | GoogleTest 单元测试框架 |

`cpp-yyjson` 使用仓库内的 `vcpkg-overlay-ports/cpp-yyjson`，由 `vcpkg-configuration.json` 自动登记。该 port 固定到源码提交 `de04a517b76c302bdfcc0ff9f96d98908239af21`。

`abseil` 和 Google `benchmark` 由可选的 `benchmarks` manifest feature 提供，仅在开启基准测试时安装。以前列出的 `vincentlaucsb-csv-parser`、`drogon` 和 `fast-float` 未参与当前构建，因此不包含在清单中。

## 安装 vcpkg

默认将 vcpkg 安装在 `~/vcpkg`。如果该目录已有可用的 vcpkg，可跳过克隆和初始化：

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
```

设置环境变量；如需在新终端中继续使用，可将以下内容加入 `~/.zshrc` 或 `~/.bashrc`：

```bash
export VCPKG_ROOT="$HOME/vcpkg"
export PATH="$VCPKG_ROOT:$PATH"
```

nova 的 CMake 配置优先读取 `VCPKG_ROOT`，未设置时默认使用 `$HOME/vcpkg`；也可以通过 `-DCMAKE_TOOLCHAIN_FILE=...` 指定其他工具链路径。

## 使用 manifest 模式构建 nova

nova 已提交 `vcpkg.json` 和 `vcpkg-configuration.json`，无需再次运行 `vcpkg new`。CMake 加载 vcpkg 工具链后会检测清单，并自动安装依赖。官方包的版本基线由 `vcpkg.json` 中的 `builtin-baseline` 固定。

在 nova 仓库根目录执行：

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure

cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

默认构建包含 nova 库、示例程序和单元测试。可以通过 `-DBUILD_TESTING=OFF` 关闭测试目标；当前清单仍会安装 `gtest`。

依赖分别安装到 `build/debug/vcpkg_installed` 和 `build/release/vcpkg_installed`，不会使用 `~/vcpkg/installed` 中的 classic 模式安装结果。vcpkg 的二进制缓存可以复用已构建的依赖。

若已有构建目录使用 classic 模式，请改用全新目录（例如 `build/manifest-debug`），避免旧缓存继续引用 classic 安装路径。

需要显式选择目标平台时，在首次配置中添加对应参数：

```bash
# Apple Silicon macOS
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug \
  -DVCPKG_TARGET_TRIPLET=arm64-osx

# x86-64 Linux
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug \
  -DVCPKG_TARGET_TRIPLET=x64-linux
```

也可以使用构建脚本；脚本只执行构建，测试仍需运行上面的 `ctest` 命令：

```bash
./build.sh          # 依次构建 Debug 和 Release
./build.sh debug    # 仅构建 Debug
./build.sh release  # 仅构建 Release
```

## 基准测试

基准实验统一在 `benchmark` 分支开发，按需选择进入 `main`。目录索引与约定见 [benchmark/README.md](benchmark/README.md)。

`benchmark/ordered_set` 使用 Google Benchmark 比较 `absl::btree_set<int32_t>`、作者的 AVX2 B-Tree set 与 `std::set<int32_t>` 的插入、命中/未命中/混合查找和删除。

```bash
cmake -S . -B build/set-release -DCMAKE_BUILD_TYPE=Release -DNOVA_BUILD_BENCHMARKS=ON
cmake --build build/set-release --target nova_ordered_set_benchmark nova_ordered_set_test --parallel
ctest --test-dir build/set-release -R nova_ordered_set_correctness --output-on-failure
build/set-release/benchmark/ordered_set/nova_ordered_set_benchmark --benchmark_filter='/100000$'
```

作者实现需要 x86-64 AVX2/POPCNT；Apple Silicon 上仅测试 Abseil 和 STL。完整口径、性能结果与 JSON 汇总方法见 [benchmark/ordered_set/README.md](benchmark/ordered_set/README.md)。

## 维护依赖清单

在 nova 仓库根目录添加依赖，然后同步修改对应的 CMake 配置：

```bash
vcpkg add port <包名>
```

重新运行 CMake 即可自动安装新增依赖。也可以单独验证清单：

```bash
vcpkg install --triplet arm64-osx
```

此命令使用 manifest 模式，依赖默认安装到仓库根目录的 `vcpkg_installed/`；它与 CMake 构建目录中的安装位置不同。Linux 上按目标架构替换 triplet，例如 `x64-linux`。

作为其他 CMake 项目的子项目使用时，由顶层项目自己的 manifest 管理依赖；vcpkg 不会自动合并 nova 的清单，`cpp-yyjson` 仅供 nova 顶层示例使用。

## 已验证环境

2026-09-18 在 macOS 26.6.2、AppleClang 21、CMake 3.31.6、Ninja 1.12.1 和 `arm64-osx` 上验证：

- 清除 `VCPKG_ROOT` 后，CMake 自动使用 `~/vcpkg`，在全新构建目录中按 manifest 安装依赖。
- 独立执行 `vcpkg install --triplet arm64-osx` 成功。
- Debug 和 Release 全部目标编译通过，两个配置下的单元测试均为 2/2 通过。
- 两个配置下的 `yyjson_demo` 和 `tomlplusplus_demo` 均运行成功。
- `cpp-yyjson` 来自本地 overlay 安装结果，没有触发 CMake 的 FetchContent 回退。

本次未验证 Linux 或 Windows；上面的 Linux triplet 命令仅作为配置示例。

参考：[vcpkg manifest 模式](https://learn.microsoft.com/en-us/vcpkg/concepts/manifest-mode)。
