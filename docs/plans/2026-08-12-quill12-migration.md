# Nova Quill 12 迁移计划

## 目标

让 Nova 在 Linux `x64-linux` 环境中使用 Quill 12.1.0 完成配置、编译和日志 focused verification，并明确向 consumer 传播 Quill 的 CMake usage requirements。

## 非目标

- 本轮不修改 Aquila、Astra 或其他 Nova consumer。
- 本轮不同时兼容多个 Quill major；旧 consumer 继续固定旧 Nova commit。
- 本轮不声明性能改善，也不发送真实订单或修改任何交易运行配置。
- 本轮不全面迁移 Nova 的其他第三方依赖或重构日志公开 API。

## 已知基线

- Nova README 记录的 vcpkg commit `96d5fb3de135b86d7222c53f2352ca92827a156b` 对应 Quill 9.0.2。
- 当前 `/home/liuxiang/vcpkg` commit 为 `aae277acf4e7de287ddb5e208b5316614de6aad7`，已安装 Quill 12.1.0。
- 当前失败发生在 `BackendOptions::cpu_affinity`：Nova 传入单个 `uint16_t`，Quill 12 要求 CPU 列表。

## 关键决策

- Quill 12 中把 Nova 的单 CPU 配置映射为零个或一个元素的 affinity list，保留现有 Nova 配置 contract。
- `std::numeric_limits<uint16_t>::max()` 继续表示“不绑定 CPU”，映射为空 list；有效 CPU 映射为单元素 list。
- 因 Nova 公共头文件暴露 Quill 类型和 macros，`nova` target 必须 `PUBLIC` 链接 `quill::quill`；本轮不尝试隐藏该 public dependency。
- 使用 fresh out-of-tree build 复现与验证，避免旧 CMake cache 掩盖依赖选择。

## 实施步骤

1. 用当前 Quill 12.1.0 在 fresh Debug build 中记录编译失败。
2. 增加覆盖默认 affinity 和显式单 CPU affinity 映射的 focused test/consumer check。
3. 最小修改日志 backend options 适配 Quill 12。
4. 修正 `nova` target 对 Quill 的 PUBLIC dependency contract。
5. 运行格式检查、Debug/Release build、focused logging test 和相关 examples。
6. review 完整 diff、公共依赖传播、禁用 affinity 语义和 shutdown 行为。

## 验证策略

- `cmake` fresh configure 必须解析到 `/home/liuxiang/vcpkg` 的 Quill 12.1.0。
- 修复前失败必须稳定出现在 `BackendOptions::cpu_affinity` 类型不匹配处。
- 修复后 Nova library、`nova_demo` 和 logging example 必须完成编译。
- focused test 必须验证默认配置不绑定 CPU、显式 CPU 生成单元素 affinity。
- Debug 和 Release 均需构建；运行验证不得依赖真实交易系统。

## 回滚

consumer 继续固定迁移前的 Nova commit 和旧 vcpkg baseline。Nova 本次改动保持为单个原子 commit，可通过恢复旧 commit 回滚。

## 未决风险

- Quill 9 到 12 可能存在 `cpu_affinity` 之外的 API 或行为变化，完整编译和 logging smoke 可能暴露后续差异。
- Nova 的 Quill 类型已进入公开头文件，因此本次迁移是 source/ABI compatibility change；consumer 必须与同一 Quill 版本重新编译。
- 本轮不包含延迟 benchmark，不能宣称 Quill 12 对日志 hot path 的延迟有改善。

## 验证结果

- fresh Debug build 在修改前稳定失败于 `BackendOptions::cpu_affinity` 的 scalar-to-vector 类型不匹配。
- 修改后 Debug 和 Release 全量 build 均成功，包含 Nova library、tests 和所有 examples。
- Debug/Release focused tests 均为 2/2 passed，覆盖默认不绑核与显式单 CPU 绑定。
- `log_demo` 使用 Quill 12.1.0 成功完成 backend 启动、CPU 5 affinity、并发日志和 shutdown，退出码为 0。
- `nova` target 的 `PUBLIC quill::quill` 传播由 focused test 的 compile/link 验证。
- 完整 build 仍显示既有 `cpp-yyjson` 第三方 header warnings；未发现 Nova 或 Quill 12 新 warning。
- 本轮未执行延迟 benchmark，也未验证 Aquila 等外部 consumer；这些验证留给后续逐项目迁移。
