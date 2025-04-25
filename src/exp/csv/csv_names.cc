#include "nova/exp/csv/csv_names.h"

std::vector<std::string> ColNames::GetColNames() const {
  return col_names;
}

void ColNames::SetColNames(const std::vector<std::string>& names) {
  col_names = names;
  for (size_t i = 0; i < names.size(); ++i) {
    col_pos[names[i]] = i;
  }
}

int ColNames::IndexOf(std::string_view name) const {
  auto it = col_pos.find(name.data());
  if (it != col_pos.end()) {
    return it->second;
  }
  return CSV_NOT_FOUND;
}

size_t ColNames::Size() const noexcept {
  return col_names.size();
}