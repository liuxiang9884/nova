//
// Created by liuxiang on 2025/3/19.
//

#include <iostream>
#include <string>

#include <toml++/toml.h>

#include "nova/format/toml.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fmt::println("Usage: tomlplusplus_demo filename");
  }

  const auto filename = argv[1];
  fmt::println("input_file = {}", filename);
  const auto toml = toml::parse_file(filename);

  const std::string_view title = toml["title"].value_or("default");
  const int32_t version = toml["value"].value_or(0);
  const float pi = toml["pi"].value_or(3.14);
  const bool debug = toml["debug"].value_or(true);
  fmt::println("title = {}, version = {}, pi = {}, debug = {}", title, version,
               pi, debug);

  const auto created_at = toml["created_at"].value<toml::date_time>().value();
  fmt::println("created_at = {}", created_at);

  const auto colors = toml["colors"].as_array();
  for (const auto& color : *colors) {
    fmt::println("{}", color.value<std::string>().value());
    fmt::println("{}", color.as_string()->get());
  }
  int a = 0;
  fmt::println("a = {}", a);
  const auto database = toml["database"];
  const auto server = database["server"].value_or("localhost");
  const auto max_connection = toml["max_connection"].value_or(0);
  const auto enable = toml["enable"].value_or(true);
  fmt::println("server = {}, max_connect = {}, enable = {}", server,
               max_connection, enable);
  std::cout << database["ports"] << "\n";

  const auto user = toml["user"];
  fmt::println("username = {}", user["name"].value_or("anonymous"));
  const auto preferences = user["preferences"];
  fmt::println("preferences.theme = {}, preferences.font_size = {}",
               preferences["theme"].value_or("default"),
               preferences["font_size"].value_or(10));

  if (const auto products = toml["products"].as_array()) {
    for (auto& item : *products) {
      if (const auto table = item.as_table()) {
        const auto name = table->at("name").value_or("anonymous");
        const auto sku = table->at("sku").value_or(0);
        fmt::println("product.name = {}, product.sku = {}", name, sku);

        if (const auto color = table->get_as<std::string>("color")) {
          fmt::println("color = {}", color->get());
        }
      }
    }
  }

  std::cout << toml::json_formatter{toml} << std::endl;
  return 0;
}