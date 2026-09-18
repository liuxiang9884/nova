# sz_45 / Ryzen 9 9950X 测试结果

2026-09-18 按用户指定机器重新运行。本页与 CSV 均为 sz_45 的数据；旧 Intel i9-10980XE 结果由 Git 历史保留，不作为当前主结果。

## 环境与方法

- SSH 别名 `sz_45`，主机名 `dctp`，Linux x86-64，AMD Ryzen 9 9950X，16 核 / 32 线程。
- 固定 CPU 8，位于第二个 CCD；SMT 同胞为 CPU 24。L3 合计 64 MiB（两个 32 MiB 实例），当前 CPU 所在 CCD 的 L3 为 32 MiB。未隔离整核或停止其他服务。
- 这是 **9950X，非作者的 9950X3D**；不能视为完全相同的硬件环境。
- 测试源码提交 `02cb2bd6021708e095a16a23de72738247876c13`，本轮不修改算法或测试代码。
- GCC 13.3.0，Release（`-O3 -DNDEBUG`），三者统一 `-mavx2 -mpopcnt`；编译命令已核对。
- Abseil `20260107.1#3`，Google Benchmark `1.9.5`；vcpkg baseline `114d9fe62faf35856b45cf55cb93b57028a45d63`，使用独立 manifest 安装目录。
- 正式测量开始时间：2026-09-18 13:39:56 +08:00。45 组，每组 5 次重复，每次目标计时至少 0.1 秒，启用 random interleaving；构建、正确性和 sanitizer 检查完成后才开始测量。
- governor 为 `powersave`，CPU scaling 与 ASLR 开启；未修改系统设置。最大 CPU 时间变异系数约 12.61%（`absl_btree_set/insert/1024`），百万键 Abseil 插入约 7.35%，不根据微小差异下结论。
- 固定种子、正负互异 int32 键，查询分 100% / 0% / 50% 命中。数据准备、构造与销毁排除；作者节点池预分配也被排除，Abseil/STL 的插入分配计入。其余口径见 README。

## 结果

CPU ns/op；各次重复的中位数，数值越小越好。

| N | 操作 | absl_btree_set | author_btree_avx2 | std_set |
| ---: | --- | ---: | ---: | ---: |
| 1,024 | insert | 18.13 | 8.89 | 12.38 |
| 1,024 | find_hit | 8.54 | 3.97 | 3.16 |
| 1,024 | find_miss | 8.09 | 4.00 | 3.13 |
| 1,024 | find_mixed | 7.76 | 4.01 | 3.20 |
| 1,024 | erase | 14.14 | 11.71 | 12.42 |
| 100,000 | insert | 53.56 | 14.94 | 82.80 |
| 100,000 | find_hit | 37.97 | 9.18 | 103.17 |
| 100,000 | find_miss | 36.03 | 9.40 | 102.34 |
| 100,000 | find_mixed | 38.95 | 9.31 | 105.80 |
| 100,000 | erase | 46.48 | 23.58 | 121.44 |
| 1,000,000 | insert | 68.71 | 25.26 | 152.09 |
| 1,000,000 | find_hit | 52.90 | 18.64 | 282.18 |
| 1,000,000 | find_miss | 52.76 | 18.99 | 263.95 |
| 1,000,000 | find_mixed | 55.25 | 18.84 | 274.94 |
| 1,000,000 | erase | 61.99 | 36.34 | 272.36 |

机器可读摘要见 [results-sz45-9950x.csv](results-sz45-9950x.csv)，包含每组中位数、变异系数及重复次数。原始 JSON 中 225 次测量均无 error/skip，单位换算与 `seconds_per_op` 已逐条交叉验证。

在此环境与口径下，作者实现在 100,000 和 1,000,000 个键的五项操作中最快；1,024 个键的三项查找则是 STL 更快。不能据此推广到任意 key、分配器或混合工作负载。与作者原文的 rand()、五次最小值统计、缓存和编译环境不同，未宣称复现原文绝对耗时。

## 验证与复现位置

- sz_45 Release：10 项 set/data 正确性测试通过；包含 nova 原有日志测试在内的 CTest 3/3 通过。
- sz_45 Debug + ASan/UBSan：同样 10 项测试通过，无内存或未定义行为报告。作者代码直接包含在测试编译单元中；依赖库本身并非全面 sanitizer 重编译。
- sz_45 作者 N=10,000,000 的五组单轮 smoke 通过，查询与删除计数正确，覆盖完整 64 键 AVX2 扫描路径。单轮结果不纳入重复性能表；Abseil/STL 千万规模本轮未运行。
- 此前 macOS ARM64 正确性、CTest、benchmark smoke，以及默认关闭 benchmark 的配置检查仍对应同一份测试代码；本轮没有将其当作指定机器结果。

sz_45 上完整产物保留于：

```text
/home/liuxiang/tmp/nova-int32-set-9950x-20260918/
  src/                       # 固定提交的源码快照
  release/                   # Release 构建
  asan/                      # ASan/UBSan 构建
  verify-and-run.sh          # sanitizer 和测量的完整执行命令
  environment.txt
  results-sz45-9950x.json
  author-large-smoke.json
  ctest.log
  asan-test.log
```

本地原始 JSON 与日志副本位于 `/Users/liuxiang/tmp/nova-int32-set-benchmark/sz45/`。JSON context 明确记录机器别名、CPU 型号、绑定 CPU 与源码提交。远程原有 `~/dev/nova` 与 `~/vcpkg` checkout 的分支/提交及工作区保持不变。

作者原始 `test_time_btree.cpp` SHA-256：`074f9e798915d1d728f4c657bfc2bdc713ed07148d5b9db7e5a0bec061dd1714`。树算法与固定原文一致，本轮没有算法修正。
