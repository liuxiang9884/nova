# nova

Nova 是供低延迟 C++ 系统复用的基础库。library 的直接依赖为 fmt、magic_enum、Quill 和 tomlplusplus；demo 和 test 另外使用 CLI11、cpp-yyjson 和 GTest。

## 作为 CMake subproject 使用

Nova 不维护自己的顶层 `vcpkg.json`。使用 Nova 的 application 应通过自己的 manifest 统一固定依赖版本，并在首次 `project()` 前提供 vcpkg toolchain。

consumer 的基础依赖示例：

```json
{
  "dependencies": [
    "fmt",
    "magic-enum",
    "quill",
    "tomlplusplus"
  ]
}
```

通过 `FetchContent` 固定完整 commit：

```cmake
include(FetchContent)

FetchContent_Declare(
    nova
    GIT_REPOSITORY git@github.com:dcfintech/nova.git
    GIT_TAG <full-commit-sha>
)
FetchContent_MakeAvailable(nova)

target_link_libraries(your_target PRIVATE nova::nova)
```

本地联调时使用 CMake 内建 override，不需要修改 `GIT_TAG`：

```bash
cmake -S . -B build \
  -DFETCHCONTENT_SOURCE_DIR_NOVA=/absolute/path/to/nova
```

Nova 作为 subproject 时不会创建自己的 demo、examples 或 tests，也不会修改 consumer 的 toolchain、triplet、编译标准或 platform 设置。

## 独立构建

准备 vcpkg：

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"
```

当前 Linux classic-mode 参考环境：

- vcpkg commit：`aae277acf4e7de287ddb5e208b5316614de6aad7`
- triplet：`x64-linux-dynamic`
- Quill：`12.1.0`

先安装 library、demo 和 test 所需依赖：

```bash
"$VCPKG_ROOT/vcpkg" install \
  cli11 fmt gtest magic-enum quill tomlplusplus \
  --triplet x64-linux-dynamic
```

`yyjson_demo` 会在没有 `cpp-yyjson` package 时使用固定 commit 的 `FetchContent` fallback。也可以先安装本仓库的 overlay port：

```bash
"$VCPKG_ROOT/vcpkg" install cpp-yyjson \
  --overlay-ports="$PWD/vcpkg-overlay-ports" \
  --triplet x64-linux-dynamic
```

执行 Debug 和 Release 构建：

```bash
./build.sh
```

也可以只构建一个配置：

```bash
./build.sh debug
./build.sh release
```
