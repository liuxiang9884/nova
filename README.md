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

使用manifest模式
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
```

使用经典模式

```bash
    vcpkg install cli11 fmt magic-enum quill tomlplusplus
```
