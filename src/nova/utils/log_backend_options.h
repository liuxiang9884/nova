#pragma once

#include <cstdint>
#include <limits>

#include <quill/backend/BackendOptions.h>

namespace nova::detail {

inline void SetBackendCpuAffinity(quill::BackendOptions& backend_options,
                                  std::uint16_t cpu_affinity) {
  backend_options.cpu_affinity.clear();
  if (cpu_affinity != std::numeric_limits<std::uint16_t>::max()) {
    backend_options.cpu_affinity.push_back(cpu_affinity);
  }
}

}  // namespace nova::detail
