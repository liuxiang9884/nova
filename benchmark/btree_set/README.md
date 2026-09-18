# 有序集合对比

本实验位于 `benchmark` 分支，按有序集合这一主题组织；当前用例的键类型为 `int32_t`。

使用 Google Benchmark 比较 `absl::btree_set<int32_t>`、Haoqiang Fan 的 64 阶 AVX2 B-Tree set、`std::set<int32_t>` 和本实验自主实现的四层 `RadixBitmapSet`。不包含 map/value。

## 构建

在 nova 根目录执行，vcpkg 默认位于 `~/vcpkg`：

```bash
cmake -S . -B build/set-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DNOVA_BUILD_BENCHMARKS=ON
cmake --build build/set-release --target nova_btree_set_benchmark nova_btree_set_test --parallel
ctest --test-dir build/set-release -R nova_btree_set_correctness --output-on-failure
```

`NOVA_BUILD_BENCHMARKS` 默认关闭。开启时，nova 的 vcpkg manifest 自动选择 `benchmarks` feature，安装 `abseil` 与 `benchmark`；不使用 classic 安装目录。GoogleTest 使用 nova 已有依赖。

作者源码需要 x86-64、AVX2 和 POPCNT，本目录在 x86-64 GCC/Clang 下默认开启 `NOVA_SET_BENCHMARK_AVX2`，四个实现统一使用 `-mavx2 -mpopcnt`。运行前确认 CPU 支持这些指令，旧 x86 CPU 可显式传入 `-DNOVA_SET_BENCHMARK_AVX2=OFF`。Apple Silicon 等平台注册 Abseil、STL 和 radix bitmap；CMake 与结果 context 都明确提示作者实现缺席，不用标量版本代替作者 AVX2 实现。

## 运行与读数

当前指定测试机器为 **`sz_45`（AMD Ryzen 9 9950X）**。正式对比在该机器执行，固定 CPU 8（第二 CCD，SMT 同胞为 CPU 24）；选择其他测试机器前应先由用户确认。

```bash
# 在 sz_45 上执行。
taskset -c 8 build/set-release/benchmark/btree_set/nova_btree_set_benchmark \
  --benchmark_filter='/(1024|100000|1000000)$' \
  --benchmark_min_time=0.1s --benchmark_repetitions=5 \
  --benchmark_enable_random_interleaving=true \
  --benchmark_display_aggregates_only=true \
  --benchmark_context=machine=sz_45,cpu=Ryzen_9_9950X,pinned_cpu=8 \
  --benchmark_out="$HOME/tmp/btree-set-results.json"

python3 benchmark/btree_set/summarize.py "$HOME/tmp/btree-set-results.json"
```

输出目录需预先存在。本地 macOS 可去掉 `taskset -c 8` 做可移植部分的 smoke，但不将其作为指定机器的性能结果。不指定 filter 会包含 10,000,000 元素用例，运行时间和内存占用明显增加。快速检查可使用 `--benchmark_filter='/1024$' --benchmark_min_time=0.001s`；这种短跑不用于性能结论。

每次 Google Benchmark iteration 是 **N 次容器操作的一整批**。`Time`/`CPU` 列单位为毫秒，表示整批耗时；`items_per_second` 是操作吞吐率，`seconds_per_op` 是按 CPU 时间归一化的每操作秒数，控制台会自动显示为 ns 等单位。汇总脚本输出各次重复的 **CPU ns/op 中位数**，越小越好；JSON 中保留每次重复和变异系数等统计。存在错误/跳过记录时，脚本拒绝输出比较表。

## 测试口径

| 用例 | 初始状态 | 计时内容 |
| --- | --- | --- |
| `insert` | 空集合 | 插入 N 个互异键 |
| `find_hit` | N 个键 | N 次查询，100% 命中 |
| `find_miss` | N 个键 | N 次查询，0% 命中 |
| `find_mixed` | N 个键 | N 次查询，50% 命中 |
| `erase` | N 个键 | 按独立打乱的顺序成功删除全部 N 个键 |

- 规模：1,024、100,000、1,000,000、10,000,000。
- 通过可逆的 32 位混合函数映射偶数输入为插入键、奇数输入为缺失键，保证唯一且互不重合；结果按位解释为 `int32_t`，包含正负数。不是从小范围连续整数中抽样，也不是原文章的 `rand()` 数据分布。
- 每个规模、每种实现使用完全相同的插入、查询和删除序列。固定 `mt19937` 种子与显式 Fisher–Yates 打乱；混合查询再打乱，避免简单的交替命中模式。
- 数据生成、容器构造、查找/删除前的填充、容器销毁均不计时。插入/删除通过 `PauseTiming/ResumeTiming` 排除准备和销毁；查找的集合在循环外构造。
- **作者节点池在构造时预分配，其分配和初始化被排除；Abseil/STL 插入过程中的实际分配仍计入。Radix 的常驻目录构造被排除，但插入时的页/叶分配、初始化和移动，以及删除时的叶移动和空页释放都计入。** 这是保留原实现分配策略的操作成本对比，不是公平化 allocator 后的算法成本，也不是端到端建表成本。
- 使用 `ClobberMemory` 和结果累积防止优化器消除工作；查询/删除返回计数异常会标记 benchmark 错误。独立正确性测试覆盖完整键边界、重复、节点分裂/合并、缺失删除和随机批次；radix 还覆盖全部层级的位边界、完整页、随机交错增删、有符号 `lower_bound` 和页回收。
- 不主动清空 CPU cache；数据准备、先前操作和重复运行会影响缓存/allocator 状态。数值是单线程批量平均成本，不代表单次请求尾延迟，也不代表混合读写、重复插入或持续服务性能。
- 不修改 CPU governor、Turbo、ASLR 等系统设置；正式比较应检查 JSON 的环境和方差，避免其他任务并发干扰。

## 作者实现来源与边界

`author_btree.hpp` 来源：

- [知乎原文](https://zhuanlan.zhihu.com/p/2006092283301352464)
- [完整 gist](https://gist.github.com/fanhqme2/df3d885ba303ec5e25ac5ffa519ca8b4)
- 固定 gist revision：`2ccbb7ca56cd071b3d83852ceaf090d2e558628b`
- 原文件：`test_time_btree.cpp`，原始内容 SHA-256 见下方验证记录。

适配仅移除原有 main/计时驱动与无关 include，增加命名空间、头文件保护和 32 位 int 检查，并清理行尾空白。键数组、AVX2 查找/插入/删除、节点池和树平衡算法保持原样。保留作者署名；上游 gist 未附独立许可证。

作者节点池按预期插入规模一次性分配，合并后不回收节点索引；不把它当作可无限交错增删的生产容器。这里每轮新建集合，只执行一次批量插入及至多一次批量删除，符合其容量使用方式。Radix bitmap 为独立实现，不改动此作者代码。

## 四层 radix bitmap 的实现

`radix_bitmap_set.hpp` 是本实验自主实现。作者未提供可核对的 radix 源码；这里的布局和优化不代表复现作者算法。逐轮实验见 [OPTIMIZATION.md](OPTIMIZATION.md)。

当前布局采用 **8+8+10+6**，按需分配页，在紧凑叶与直接索引叶之间自适应选择：

- `int32_t` 转无符号后 XOR `0x80000000`，位序与有符号大小顺序一致。完整域、重复插入及缺失删除均受支持，不要求键位于小范围。
- 前两层为 256 位 root 和 256 个 256 位 group 摘要。高 16 位还直接索引 65,536 个页指针；精确查询不必遍历上层摘要。
- 每页覆盖 65,536 个可能键。第三层 1,024 位记录哪些 64 位叶字存在，第四层按第三层位序紧凑存储有效叶字。空叶无需占一个固定 64 位槽。
- 稀疏模式下，每 64 个第三层摘要位保存一个 16 位前缀计数；`rank = prefix + popcount(当前摘要字中目标之前的位)`，定位紧凑叶数组。
- 稀疏页内联 16 个叶字，超过后容量按倍数增长至 64。新增第 65 个非空叶字时，分配并清零 1,024 个直接索引叶字，将原有位复制过去，然后释放紧凑数组的溢出存储。扩容、转换和初始化均在插入计时区间内，未借助构造参数预分配。
- 稀疏模式新增或移除叶字时更新后续前缀。编译开启 AVX2 时用一次 256 位向量操作更新 16 个计数；其余平台使用标量循环，功能与数据布局一致。稠密模式直接索引叶字，不移动数组、不维护或读取已失效的前缀计数。
- `LowerBound` 使用四层摘要、rank 和 `std::countr_zero` 跳过空范围；没有更改有符号排序语义。当前 benchmark 仍测精确成员查询，`LowerBound` 仅做正确性验证。

API 为 `Insert`（是否新增）、`Contains`、`Erase`（是否删除）、`Size`、返回 `std::optional<int32_t>` 的 `LowerBound`，以及诊断用 `StorageBytes`。不提供 STL 迭代器、复制/移动或并发修改接口。

页和扩容数组由 RAII 管理，分配成功后才发布摘要和计数；最后一个键删除后整页立即回收。数组在页非空时保留历史容量，稠密页也不降级，避免反复收缩或转换。极密集数据仍可占用数百 MiB；转换阈值影响吞吐与空间，因此不能假定所有数据规模均受益。

`StorageBytes()` 和查询 JSON 中的 `storage_bytes` 包含对象、页与叶数组已分配容量，不含 allocator 元数据/保留内存、数据集和其他容器，不是进程 RSS。内联空间计入页大小，溢出数组另计，避免重复统计。存储统计发生在查询计时之前，不计入查找成本。

## 验证与结果

实测环境、验证范围和性能表见 [RESULTS.md](RESULTS.md)。
