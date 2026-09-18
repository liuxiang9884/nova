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
- **作者节点池在构造时预分配，其分配和初始化被排除；Abseil/STL 插入过程中的实际分配仍计入。Radix 的常驻目录构造被排除，但插入时的位图页分配/清零、删除时的空页释放都计入。** 这是保留原实现分配策略的操作成本对比，不是公平化 allocator 后的算法成本，也不是端到端建表成本。
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

`radix_bitmap_set.hpp` 是本实验的独立实现。作者最新说明未提供可核对的 radix 源码，因此这里的 **8+8+8+8 划分和页布局是我们的设计**，不用于验证作者声称的约 10 倍收益。

- 将 `int32_t` 转为无符号后 XOR `0x80000000`，使位序与有符号数值顺序一致；接受完整值域及重复键，不要求预先知道键范围。
- 第一层 256 位，第二层共 256 × 256 位，记录非空高位前缀。高 16 位还直接索引 65,536 个页指针，成员查询跳过摘要遍历。
- 每个页覆盖 65,536 个可能键，含第三层 256 位摘要和第四层 65,536 位数据，共 8,224 字节；页在首次插入时分配并清零，最后一个键删除后立即释放。
- `Contains` 通过页指针和叶位直接查询；`LowerBound` 使用四层摘要和 `std::countr_zero` 跳过空范围，无比较器及树平衡。
- 支持 `Insert`（是否新增）、`Contains`、`Erase`（是否删除）、`Size` 和返回 `std::optional<int32_t>` 的 `LowerBound`。不提供 STL 迭代器、复制、移动和并发修改接口，当前是 benchmark 实验容器。
- 页使用 RAII 管理。分配失败发生在摘要/计数更新之前，不改变集合内容。可重复增删并回收空页，不使用作者 B-Tree 的固定节点池容量规则。

**内存是这一版的主要代价。** 64 位平台的空集合目录约 520 KiB；全覆盖约 514.51 MiB。即使只存一百万个随机键，几乎所有高 16 位前缀也会出现，空间接近上限。少量键若分散到不同页也会浪费空间；不能把“小范围密集整数”的位图优势直接推广到此处的完整域随机数据。后续可以另外比较稀疏叶、自适应页或紧凑索引；本轮不把这些未实现的改进计入结论。

查找用例的 JSON `storage_bytes` 记录 radix 对象和当前活页的合计字节数（没有跨 iteration 累加），不含 allocator 元数据、释放后 allocator 保留的内存、测试数据或其他容器；它不是进程 RSS。当前性能表仍只测精确成员查询、插入和删除，`LowerBound` 仅做正确性验证。

## 验证与结果

实测环境、验证范围和性能表见 [RESULTS.md](RESULTS.md)。
