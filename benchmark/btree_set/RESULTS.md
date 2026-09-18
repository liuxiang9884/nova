# sz_45 / Ryzen 9 9950X 四种 set 的测试结果

2026-09-18 新增自主实现的 radix bitmap set 后，同次重跑四种实现。本页和 CSV 全部来自 sz_45；旧三种实现的结果留在 Git 历史中，未混入本轮比较。代码仅保留在 `benchmark` 分支。

## 环境与方法

- SSH 别名 `sz_45`，主机名 `dctp`，Linux x86-64，AMD Ryzen 9 9950X，16 核 / 32 线程；不是作者使用的 9950X3D。
- 固定 CPU 8，SMT 同胞 CPU 24；该 CCD 的共享 L3 为 32 MiB，全 CPU 合计 64 MiB。没有隔离整核或停止其他服务。
- 测量源码：`703431e2165c7e04236ae2b58c49739684cbb461`。其后提交只更新文档与结果，不改变被测代码。
- GCC 13.3.0，Release `-O3 -DNDEBUG`，四种实现统一 `-mavx2 -mpopcnt`，C++20；未使用 `-march=native`。
- Abseil `20260107.1#3`，Google Benchmark `1.9.5`，vcpkg baseline `114d9fe62faf35856b45cf55cb93b57028a45d63`，复用此前的独立 manifest 安装目录。
- 正式测量开始：2026-09-18 15:52:53 +08:00。60 组 × 5 次重复，每次目标计时至少 0.1 秒，启用 random interleaving；构建、Release 和 sanitizer 检查完成后才开始测量。
- governor 为 `powersave`，CPU scaling 和 ASLR 开启；未修改系统设置。最大 CPU 时间变异系数 11.99%（radix 百万键混合查询）；radix 百万键删除 9.96%，因此不把它与作者删除的几个百分点差异视为确定性优劣。
- 固定种子、完整域正负互异 `int32_t` 键；100% / 0% / 50% 命中。数据生成、容器构造、准备和销毁排除。作者预分配节点池、radix 常驻目录的构造成本均排除；radix 插入时的页分配/清零和删除时的页释放计入。不是统一 allocator 或端到端建表成本比较。

## 结果

CPU ns/op；各次重复的中位数，数值越小越好。

| N | 操作 | absl_btree_set | author_btree_avx2 | radix_bitmap_set | std_set |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1,024 | insert | 16.25 | 8.84 | 71.28 | 12.26 |
| 1,024 | find_hit | 7.69 | 3.99 | 1.12 | 3.40 |
| 1,024 | find_miss | 8.18 | 3.99 | 0.42 | 3.13 |
| 1,024 | find_mixed | 8.04 | 4.01 | 0.70 | 3.11 |
| 1,024 | erase | 13.72 | 11.66 | 18.76 | 12.20 |
| 100,000 | insert | 52.20 | 14.97 | 586.48 | 82.88 |
| 100,000 | find_hit | 36.47 | 9.13 | 2.85 | 102.57 |
| 100,000 | find_miss | 36.31 | 9.46 | 2.91 | 102.62 |
| 100,000 | find_mixed | 39.28 | 9.27 | 2.66 | 105.63 |
| 100,000 | erase | 47.53 | 24.33 | 138.73 | 120.88 |
| 1,000,000 | insert | 68.59 | 25.28 | 99.03 | 159.18 |
| 1,000,000 | find_hit | 53.08 | 18.45 | 12.24 | 305.72 |
| 1,000,000 | find_miss | 52.62 | 18.90 | 12.34 | 280.85 |
| 1,000,000 | find_mixed | 55.54 | 18.78 | 12.25 | 298.56 |
| 1,000,000 | erase | 63.34 | 37.50 | 38.50 | 282.17 |

机器可读摘要见 [results-sz45-9950x.csv](results-sz45-9950x.csv)。原始 JSON 的 300 次测量均无 error/skip，60 组均有 5 次原始重复；`CPU / N` 与 `seconds_per_op` 已逐条核对。

### 可以得出的结论

- 这一版 radix 的三种精确查找在本轮三个规模中均最快。百万键命中查询 12.24 ns/op，作者 18.45，Abseil 53.08，STL 305.72；分别约为 **1.51×、4.34×、24.97×** 的吞吐率。
- 插入有明显代价：百万键 radix 为 99.03 ns/op，作者为 25.28，耗时约 **3.92 倍**；十万键为 586.48 对 14.97，耗时约 **39.18 倍**。稀疏前缀导致大量位图页分配和清零，只有更多键共用已分配页时，每键成本才容易摊薄。未做 profile 将分配、清零、缓存失效的各项占比分开，此处是结合布局和测量的解释。
- 删除在百万键下与作者接近（38.50 对 37.50 ns/op），差异落在本轮波动范围内；十万键 radix 为 138.73，明显慢于作者 24.33，空页回收也是操作的一部分。
- 相对作者 B-Tree，百万键三种查询收益约 1.5 倍，不是统一快 10 倍。不能把这版的结果等同于作者未公开 radix 实现的表现；也不能推广到连续小范围键、其他 key 类型或混合读写工作负载。

### Radix 的内存代价

以下为 `StorageBytes()`：对象与活页的字节数，排除 allocator 开销及保留页、数据集和其他容器，不是 RSS。由三种查询的 `storage_bytes` 交叉核对。

| N | 活页及对象字节 | MiB |
| ---: | ---: | ---: |
| 1,024 | 8,912,784 | 8.50 |
| 100,000 | 422,448,400 | 402.88 |
| 1,000,000 | 539,500,592 | 514.51 |

空集合的常驻目录约 520 KiB；百万随机键已占满所有 65,536 个页。查询只触及相关 cache line，不能由分配容量推断每次查询会扫描整个 515 MiB。当前设计以空间换直接定位，后续若优化插入和内存，应另测稀疏/自适应叶布局。

## 验证与复现

- 新 API 测试先于实现添加，初次编译因缺少 `radix_bitmap_set.hpp` 失败。
- macOS ARM64 / AppleClang Release：13 项正确性测试通过（按平台条件不包含作者 AVX2 版本）。
- sz_45 GCC Release：完整构建成功，CTest 3/3 通过，其中 set 测试含 16 项；保留原有日志测试。
- sz_45 Debug + `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`：16 项通过，启用泄漏检查和 halt-on-error，无 sanitizer 报告。外部依赖并未全面以 sanitizer 重编译。
- radix 验证完整键边界、重复、缺失删除、全页位覆盖、所有上层摘要位、随机交错操作、负数顺序、`lower_bound` 与 `std::set` 对照、回收后重插与存储计数恢复。
- N=10,000,000 的四种实现 × 五种操作另做单轮 smoke，查询/删除计数检查通过。它不参与以上五次重复的性能表。
- 最终审查检查摘要更新/清理、移位边界、异常前发布顺序、所有权与计时边界；没有修改作者算法、已有数据生成或基线计时方式。

标准构建与运行命令见 [README.md](README.md)。本次由于 sz_45 默认 vcpkg 版本较旧，使用已有独立 vcpkg 工作树和 manifest 安装目录，原 `~/dev/nova`、`~/vcpkg` checkout 保持不变：

```bash
export VCPKG_ROOT="$HOME/tmp/nova-int32-set-9950x-20260918/vcpkg"
cmake -S src -B release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DNOVA_BUILD_BENCHMARKS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DVCPKG_INSTALLED_DIR="$HOME/tmp/nova-int32-set-9950x-20260918/release/vcpkg_installed"
cmake --build release --parallel 8
ctest --test-dir release --output-on-failure
```

sz_45 产物根目录：`/home/liuxiang/tmp/nova-radix-bitmap-20260918/`：

```text
src/                     # 固定提交的 git archive，无 macOS AppleDouble 文件
release/                 # Release 构建及 compile_commands.json
asan/                    # sanitizer 构建
verify-and-run.sh        # 验证、正式测量与 10M smoke 的完整命令
results-sz45-9950x.json   # 300 次正式原始测量及聚合统计
large-smoke.json          # 20 组单轮 smoke
summary.md
environment.txt
ctest.log
asan-test.log
```

本地原始结果与日志副本：`/Users/liuxiang/tmp/nova-radix-bitmap/sz45/`。JSON context 记录机器、CPU、绑定核心、布局和源码提交。

作者源码仍来自固定 gist revision `2ccbb7ca56cd071b3d83852ceaf090d2e558628b`；原始 `test_time_btree.cpp` SHA-256 为 `074f9e798915d1d728f4c657bfc2bdc713ed07148d5b9db7e5a0bec061dd1714`。
