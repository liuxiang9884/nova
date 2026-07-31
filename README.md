# nova

## 安裝與使用 vcpkg
### 安裝
1.第一個步驟是從 GitHub 複製 vcpkg 存放庫。 存放庫包含可取得 vcpkg 可執行文件的腳本，以及 vcpkg 社群所維護之策劃開放原始碼連結庫的登錄
```bash
    git clone https://github.com/microsoft/vcpkg.git
```
2.接下來，您需要使用以下命令編譯 vcpkg 可執行文件：
```bash
    cd vcpkg && ./bootstrap-vcpkg.sh
```
3.設定專案
```bash
    export VCPKG_ROOT=/path/to/vcpkg
    export PATH=$VCPKG_ROOT:$PATH
```
若要讓此變更在會話之間永久完成，請將 命令新增 export 至殼層的配置檔腳本（例如 ~/.bashrc 或 ~/.zshrc）

## 使用manifest模式
1. 建立清單
```bash
    vcpkg new --application
```
這會在當前目錄中建立一個新的 vcpkg 專案，並包含以下檔案：
- vcpkg.json : 相依性目錄
- vcpkg-configuration.json : vcpkg 設定檔

2. 添加套件相依性
```bash
    vcpkg add port <pkg_name>
```

3. 安裝套件

```bash
    vcpkg install
```

4. 已经安装的packages
```
cli11
fmt
magic-enum
quill
tomlplusplus
vincentlaucsb-csv-parser
yyjson
cpp-yyjson
nameof
drogon
fast-float
benchmark
gtest
```

## 使用经典模式

```bash
    vcpkg install cli11 fmt magic-enum quill tomlplusplus vincentlaucsb-csv-parser yyjson nameof drogon fast-float benchmark gtest
```

`cpp-yyjson` 使用本仓库提供的 vcpkg overlay port。classic mode 下先显式安装：

```bash
    cd your_path/nova
    $VCPKG_ROOT/vcpkg install cpp-yyjson --overlay-ports="$PWD/vcpkg-overlay-ports" --triplet x64-linux
```

安装完成后，CMake 配置只需要使用 vcpkg toolchain 和目标 triplet，不需要再传 overlay path：

```bash
    cmake -S . -B build/debug \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
      -DVCPKG_TARGET_TRIPLET=x64-linux
```

当前 Linux classic-mode 参考环境：

- vcpkg commit: `96d5fb3de135b86d7222c53f2352ca92827a156b`
- triplet: `x64-linux`

## 构建
```bash
    cd your_path/nova
    chmod a+x build.sh
    ./build.sh
```
默认分别建立build/debug和build/release两个folder，分别执行debug和release编译。
也可以指定debug或者release参数进行编译。
