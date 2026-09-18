# 2026-09-18 验证与实测

## 环境与方法

- Linux x86-64，Intel Core i9-10980XE，18 个逻辑 CPU，L3 约 24.75 MiB；测试绑定 CPU 16。
- GCC 13.3.0，Release（`-O3 -DNDEBUG`），三个实现统一使用 `-mavx2 -mpopcnt`。
- Abseil `20260107.1#3`，Google Benchmark `1.9.5`；vcpkg baseline `114d9fe62faf35856b45cf55cb93b57028a45d63`。
- 每组 5 次重复，每次目标计时至少 0.1 秒，启用 random interleaving；命令见 README。计时发生在 sanitizer、构建和正确性测试结束之后。
- CPU scaling 和 ASLR 开启，未更改系统设置。最大 CPU 时间变异系数约 9.13%（`std_set/find_mixed/1000000`），因此不根据微小差距下结论。
- 数据、缓存和预分配口径见 README；作者节点池构造被排除，不能把下表当作含全部分配成本的端到端结果。

## 结果

CPU ns/op；各次重复的中位数，数值越小越好。

| N | 操作 | absl_btree_set | author_btree_avx2 | std_set |
| ---: | --- | ---: | ---: | ---: |
| 1,024 | insert | 49.11 | 19.32 | 48.15 |
| 1,024 | find_hit | 31.66 | 8.26 | 39.58 |
| 1,024 | find_miss | 27.53 | 8.49 | 37.49 |
| 1,024 | find_mixed | 32.10 | 8.34 | 37.47 |
| 1,024 | erase | 46.26 | 25.35 | 69.32 |
| 100,000 | insert | 79.15 | 35.58 | 139.25 |
| 100,000 | find_hit | 61.04 | 19.03 | 140.48 |
| 100,000 | find_miss | 59.67 | 19.36 | 139.46 |
| 100,000 | find_mixed | 61.71 | 19.24 | 142.53 |
| 100,000 | erase | 73.14 | 46.50 | 170.88 |
| 1,000,000 | insert | 105.12 | 63.72 | 288.64 |
| 1,000,000 | find_hit | 84.53 | 36.94 | 372.51 |
| 1,000,000 | find_miss | 82.66 | 37.63 | 352.71 |
| 1,000,000 | find_mixed | 86.77 | 37.36 | 364.06 |
| 1,000,000 | erase | 98.12 | 80.72 | 411.35 |

机器可读摘要见 [results-linux-x86_64.csv](results-linux-x86_64.csv)，包括每组中位数、变异系数和重复次数。原始 Google Benchmark JSON 共 225 次测量，汇总无 error/skip 记录；在本次工作目录的 `final-results.json` 中保留原始输出。

在此机器与测试口径下，作者的整数 AVX2 实现在这些用例中更快；不能推广为任意 key、分配器、CPU 或混合工作负载下的排名。与文章中的硬件、rand() 分布和最小值统计也不同，不直接对照绝对用时。

## 正确性与兼容性

- Linux Release：10 项 set/data 测试全部通过；包括 nova 现有日志测试在内的 CTest 3/3 通过。
- Linux Debug + ASan/UBSan：同样的 10 项测试通过，未报告内存或未定义行为错误。被测作者代码直接包含在测试编译单元中；依赖库本身不是全面 sanitizer 重编译。
- 作者 N=10,000,000 的五组操作单轮 smoke 通过，查询与删除计数正确，覆盖大节点池使用的完整 64 键 AVX2 扫描路径；不将单轮 smoke 纳入上面的重复性能比较。Abseil/STL 的千万规模用例已注册，本轮未实测。
- macOS ARM64：Abseil/STL 的 7 项 set/data 测试通过，CTest 3/3 通过，10 组 N=1,024 benchmark smoke 通过；不注册作者 AVX2 实现，不报告跨机器比较。
- 默认关闭 benchmark 的全新 CMake 配置通过，不创建 benchmark target，也不安装可选 Abseil/Google Benchmark 依赖。
- 汇总脚本的单位换算与 Google Benchmark 的 CPU batch 时间及 `seconds_per_op` 一致；人为加入 error 的 JSON 会被拒绝。

作者原始 `test_time_btree.cpp` SHA-256：`074f9e798915d1d728f4c657bfc2bdc713ed07148d5b9db7e5a0bec061dd1714`。适配后树算法与固定原文逐 token 核对一致，仅格式变化，不含算法修正。
