#ifndef CSV_NAMES_H
#define CSV_NAMES_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int CSV_NOT_FOUND = -1;

struct ColNames {
 public:
  ColNames() = default;
  ColNames(const std::vector<std::string>& names) {
    SetColNames(names);
  }

  std::vector<std::string> GetColNames() const;
  void SetColNames(const std::vector<std::string>&);
  int IndexOf(std::string_view) const;

  bool IsEmpty() const noexcept {
    return this->col_names.empty();
  }
  size_t Size() const noexcept;

 private:
  std::vector<std::string> col_names;
  std::unordered_map<std::string, size_t> col_pos;
};

using ColNamesPtr = std::shared_ptr<ColNames>;

#endif  // CSV_FORMAT_H