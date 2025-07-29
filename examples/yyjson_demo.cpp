//
// Created by liuxiang on 2025/7/29.
//
#include <chrono>
#include <map>
#include <vector>

#include <fmt/format.h>

#include <cpp_yyjson.hpp>

namespace yy = yyjson;

void NormalMode() {
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

void InsituMode() {
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
}

int main() {
  fmt::println("=== YYJSON Demo ===");

  fmt::println("\n--- Normal Mode ---");
  NormalMode();

  fmt::println("\n--- Insitu Mode ---");
  InsituMode();

  return 0;
}