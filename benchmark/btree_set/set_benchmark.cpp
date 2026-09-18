#include <optional>
#include <string>

#include <benchmark/benchmark.h>

#include "dataset.hpp"
#include "set_adapters.hpp"

namespace nova_bench {

enum class Operation { Insert, FindHit, FindMiss, FindMixed, Erase };

void Report(benchmark::State& state, size_t n) {
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
  state.counters["N"] = static_cast<double>(n);
  // An inverted rate is formatted by Google Benchmark as seconds per item.
  state.counters["seconds_per_op"] = benchmark::Counter(
      static_cast<double>(n), benchmark::Counter::kIsIterationInvariantRate |
                                  benchmark::Counter::kInvert);
}

template <class Set, Operation Op>
void Run(benchmark::State& state) {
  const size_t n = static_cast<size_t>(state.range(0));
  const Dataset data(n);
  if constexpr (Op == Operation::Insert || Op == Operation::Erase) {
    std::optional<Set> set;
    size_t removed = 0;
    for (auto _ : state) {
      state.PauseTiming();
      set.emplace(n);
      if constexpr (Op == Operation::Erase) {
        for (auto key : data.insert) set->Insert(key);
      }
      // Escape the container before the timed writes and ClobberMemory.
      benchmark::DoNotOptimize(*set);
      state.ResumeTiming();
      if constexpr (Op == Operation::Insert) {
        for (auto key : data.insert) set->Insert(key);
      } else {
        removed = 0;
        for (auto key : data.erase) removed += set->Erase(key);
        benchmark::DoNotOptimize(removed);
      }
      benchmark::ClobberMemory();
      state.PauseTiming();
      set.reset();
      state.ResumeTiming();
    }
    if constexpr (Op == Operation::Erase) {
      if (removed != n)
        state.SkipWithError("Erase result mismatch; reject timing");
    }
  } else {
    Set set(n);
    for (auto key : data.insert) set.Insert(key);
    benchmark::DoNotOptimize(set);
    if constexpr (requires { set.StorageBytes(); }) {
      state.counters["storage_bytes"] = static_cast<double>(set.StorageBytes());
    }
    const auto& queries = Op == Operation::FindHit    ? data.hit
                          : Op == Operation::FindMiss ? data.miss
                                                      : data.mixed;
    constexpr bool hit = Op == Operation::FindHit;
    const size_t expected = hit                         ? n
                            : Op == Operation::FindMiss ? 0
                                                        : (n + 1) / 2;
    size_t found = 0;
    for (auto _ : state) {
      benchmark::ClobberMemory();
      found = 0;
      for (auto key : queries) found += set.Contains(key);
      benchmark::DoNotOptimize(found);
    }
    if (found != expected)
      state.SkipWithError("Find result mismatch; reject timing");
  }
  Report(state, n);
}

template <class Set, Operation Op>
void Register(const char* container, const char* operation) {
  const std::string name = std::string(container) + "/" + operation;
  benchmark::RegisterBenchmark(name.c_str(), &Run<Set, Op>)
      ->Arg(1'024)
      ->Arg(100'000)
      ->Arg(1'000'000)
      ->Arg(10'000'000)
      ->Unit(benchmark::kMillisecond);
}

template <class Set>
void RegisterSet(const char* name) {
  Register<Set, Operation::Insert>(name, "insert");
  Register<Set, Operation::FindHit>(name, "find_hit");
  Register<Set, Operation::FindMiss>(name, "find_miss");
  Register<Set, Operation::FindMixed>(name, "find_mixed");
  Register<Set, Operation::Erase>(name, "erase");
}

}  // namespace nova_bench

int main(int argc, char** argv) {
  using namespace nova_bench;
  RegisterSet<AbslSet>("absl_btree_set");
  RegisterSet<StdSet>("std_set");
  RegisterSet<RadixBitmapSet>("radix_bitmap_set");
#if NOVA_SET_HAS_AVX2
  RegisterSet<AuthorSet>("author_btree_avx2");
#endif
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  benchmark::AddCustomContext("key_type", "int32_t");
  benchmark::AddCustomContext(
      "radix_layout",
      "8+8+10+6; adaptive compact/dense pages; SIMD prefix; full int32 domain");
  benchmark::AddCustomContext("dataset",
                              "bijective32-even/odd; fixed seeds; unique keys");
  benchmark::AddCustomContext(
      "timing",
      "CPU batch time; setup/destruction excluded; seconds_per_op normalized");
  benchmark::AddCustomContext(
      "author_avx2",
      NOVA_SET_HAS_AVX2 ? "enabled" : "not available in this build");
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
