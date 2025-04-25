#include "nova/exp/csv/csv_row.hpp"

// inline RawCSVField& CSVFieldList::operator[](size_t n) const {
//   const size_t page_on = n / single_buffer_capacity_;
//   const size_t buffer_idx = (page_on < 1) ? n : n % single_buffer_capacity_;
//   return buffers_[page_on][buffer_idx];
// }

// inline void CSVFieldList::Allocate() {
//   buffers_.push_back(std::make_unique<RawCSVField[]>(single_buffer_capacity_));
//   current_buffer_size_ = 0;
//   back_ = buffers_.back().get();
// }

std::string_view CSVRow::GetField(size_t index) const
{
    // using internals::ParseFlags;

    if (index >= this->Size())
        throw std::runtime_error("Index out of bounds.");

    const size_t field_index = this->fields_start_ + index;
    auto& field = this->data_->fields[field_index];
    auto field_str = std::string_view(this->data_->data).substr(this->data_start_ + field.start);

    if (field.has_double_quote) {
        auto& value = this->data_->double_quote_fields[field_index];
        if (value.empty()) {
            bool prev_ch_quote = false;
            for (size_t i = 0; i < field.length; i++) {
                if (this->data_->parse_flags[field_str[i] + 128] == ParseFlags::QUOTE) {
                    if (prev_ch_quote) {
                        prev_ch_quote = false;
                        continue;
                    }
                    else {
                        prev_ch_quote = true;
                    }
                }

                value += field_str[i];
            }
        }

        return std::string_view(value);
    }

    return field_str.substr(0, field.length);
}   

CSVField CSVRow::operator[](size_t n) const {
  return CSVField(this->GetField(n));
}

CSVField CSVRow::operator[](const std::string& col_name) const {
  auto & col_names = this->data_->col_names;
  auto col_pos = col_names->IndexOf(col_name);
  if (col_pos > -1) {
      return this->operator[](col_pos);
  }

  throw std::runtime_error("Can't find a column named " + col_name);
}

CSVRow::operator std::vector<std::string>() const {
  std::vector<std::string> ret;
  for (size_t i = 0; i < Size(); i++)
    ret.push_back(std::string(this->GetField(i)));

  return ret;
}

