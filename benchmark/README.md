# 基准测试实验

本目录集中存放 nova 的 benchmark，默认在 **`benchmark` 分支**开发。实验可以长期保留在此分支；仅在用户明确选择需要进入主线的内容后，才单独合并到 `main`。

| 目录 | 比较内容 | 当前用例 |
| --- | --- | --- |
| [ordered_set](ordered_set/README.md) | Abseil B-Tree set、作者 AVX2 B-Tree set、STL set | `int32_t` 键；插入、查找、删除 |

后续实验按主题新增子目录；同一主题的不同键类型、规模和访问模式放在对应目录内扩展。

根目录通过 `-DNOVA_BUILD_BENCHMARKS=ON` 开启基准构建。具体目标、运行命令和验证结果见各实验 README。
