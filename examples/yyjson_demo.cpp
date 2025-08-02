//
// Created by liuxiang on 2025/7/29.
//
#include <chrono>
#include <map>
#include <vector>

#include <fmt/format.h>

#include <cpp_yyjson.hpp>

namespace yy = yyjson;

void Read() {
  auto json_str = R"(
{
    "id": 1,
    "pi": 3.141592,
    "name": "example",
    "array": [0, 1, 2, 3, 4],
    "currency": {
        "USD": 129.66,
        "EUR": 140.35,
        "GBP": 158.72
    },
    "success": true
})";

  auto start = std::chrono::steady_clock::now();
  auto value = yy::read(json_str);
  auto end = std::chrono::steady_clock::now();
  fmt::println("read time: {} ns",
               std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                   .count());

  auto obj = *value.as_object();

  // Key access to the JSON object class
  auto id = *obj["id"].as_int();
  auto pi = *obj["pi"].as_real();
  auto name = *obj["name"].as_string();
  auto success = *obj["success"].as_bool();
  fmt::println("id: {}, pi: {}", id, pi);
  fmt::println("name: {}", name);
  fmt::println("success: {}", success);

  const auto list = *obj["array"].as_array();
  for (const auto& v : list) {
    fmt::println("value: {}", v.write());
  }

  auto dict = *obj["currency"].as_object();
  for (const auto& [k, v] : dict) {
    fmt::println("{}: {}\n", k, v.write());
  }

  auto numbers = yy::cast<std::vector<int>>(list);
  auto currency = yy::cast<std::map<std::string_view, double>>(dict);

  fmt::println("currency: {}", obj.write());
}

void ReadInsitu() {
  std::string json_str = R"(
    {
        "id": 1,
        "pi": 3.141592,
        "name": "example",
        "array": [0, 1, 2, 3, 4],
        "currency": {
            "USD": 129.66,
            "EUR": 140.35,
            "GBP": 158.72
        },
        "success": true
    })";

  const std::string padding_str = std::string(YYJSON_PADDING_SIZE, '\0');
  json_str += padding_str;
  auto start = std::chrono::steady_clock::now();
  auto value = yy::read(json_str, json_str.size() - padding_str.size(),
                        yy::ReadFlag::ReadInsitu);
  auto end = std::chrono::steady_clock::now();
  fmt::println("read time: {} ns",
               std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                   .count());

  auto obj = *value.as_object();

  auto id = *obj["id"].as_int();
  auto pi = *obj["pi"].as_real();
  auto name = *obj["name"].as_string();
  auto success = *obj["success"].as_bool();
  fmt::println("id: {}, pi: {}", id, pi);
  fmt::println("name: {}", name);
  fmt::println("success: {}", success);

  const auto list = *obj["array"].as_array();
  for (const auto& v : list) {
    fmt::println("value: {}", v.write());
  }

  auto dict = *obj["currency"].as_object();
  for (const auto& [k, v] : dict) {
    fmt::println("{}: {}", k, v.write());
  }

  auto numbers = yy::cast<std::vector<int>>(list);
  auto currency = yy::cast<std::map<std::string_view, double>>(dict);

  fmt::println("currency: {}", obj.write());
  std::string jstr = std::string(obj.write());
  std::string_view jstr_view = obj.write();
  fmt::println("jstr: {}", jstr);
  fmt::println("jstr_view: {}", jstr_view);
}

void Write() {
  auto v_null = yy::value();  // Initial value as null
  auto v_bool = yy::value(true);
  auto v_num = yy::value(3.141592);
  auto v_str = yy::value("example");

  // Create a new empty JSON array
  auto arr = yy::array();
  arr.emplace_back(1);
  arr.emplace_back("string");

  // Create a new empty JSON object
  auto obj = yy::object();
  obj.emplace("USD", 129.66);
  obj.emplace("date", "Wed Feb 1 2023");

  // Conversion from range to JSON array class
  auto vec = std::vector{1, 2, 3};
  auto vec_nst = std::vector<std::vector<int>>{{1, 2}, {3, 4}};
  auto arr_vec = yy::array(vec);      // -> [1,2,3]
  auto arr_nst = yy::array(vec_nst);  // -> [[1,2],[3,4]]
  yy::array arr_rng =                 // transformation via range adaptors
      std::vector{1, 2, 3} |
      std::ranges::views::transform([](auto x) { return x * x; });
  // -> [1,4,9]

  // Conversion from key-value-like range to JSON object class
  auto kv_map = std::map<std::string_view, double>{
      {"first", 1.0}, {"second", 2.0}, {"third", 3.0}};
  auto val_map = std::map<std::string_view, yy::value>{
      {"number", 1.5}, {"error", nullptr}, {"text", "abc"}};
  auto obj_map = yy::object(kv_map);
  auto obj_kv = yy::object(val_map);

  // Construction by std::initializer_list
  auto init_arr = yy::array{
      nullptr, true, "2", 3.0, {4.0, "5", false}, {{"7", 8}, {"9", {0}}}};
  auto init_obj = yy::object{
      {"id", 1},
      {"pi", 3.141592},
      {"name", "example"},
      {"array", {0, 1, 2, 3, 4}},
      {"currency", {{"USD", 129.66}, {"EUR", 140.35}, {"GBP", 158.72}}},
      {"success", true}};
}

void MemoryUsageComparison() {
  fmt::println("\n=== Memory Usage Comparison ===");

  std::string json_str = R"(
    {
        "id": 1,
        "pi": 3.141592,
        "name": "example",
        "array": [0, 1, 2, 3, 4],
        "currency": {
            "USD": 129.66,
            "EUR": 140.35,
            "GBP": 158.72
        },
        "success": true
    })";

  fmt::println("Original JSON size: {} bytes", json_str.size());

  // Normal mode
  {
    auto start = std::chrono::steady_clock::now();
    auto value = yy::read(json_str);
    auto end = std::chrono::steady_clock::now();

    fmt::println("Normal mode:");
    fmt::println(
        "  Parse time: {} ns",
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count());
    fmt::println("  Memory overhead: 需要额外的字符串副本");
  }

  // Insitu mode
  {
    std::string json_copy = json_str;  // 为了公平比较
    json_copy.resize(json_copy.size() + YYJSON_PADDING_SIZE, '\0');

    auto start = std::chrono::steady_clock::now();
    auto value = yy::read(json_copy, json_copy.size() - YYJSON_PADDING_SIZE,
                          yy::ReadFlag::ReadInsitu);
    auto end = std::chrono::steady_clock::now();

    fmt::println("Insitu mode:");
    fmt::println(
        "  Parse time: {} ns",
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count());
    fmt::println("  Memory overhead: 仅 padding ({} bytes)",
                 YYJSON_PADDING_SIZE);
  }
}

int main() {
  fmt::println("=== YYJSON Demo ===");

  fmt::println("\n--- Read ---");
  Read();

  fmt::println("\n--- ReadInsitu ---");
  ReadInsitu();

  fmt::println("\n--- Write ---");
  Write();

  fmt::println("\n--- Memory Usage Comparison ---");
  MemoryUsageComparison();

  return 0;
}