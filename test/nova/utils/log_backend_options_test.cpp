#include "nova/utils/log_backend_options.h"

#include <gtest/gtest.h>

#include "nova/utils/log.h"

namespace nova::detail {
namespace {

TEST(LogBackendOptionsTest, LeavesBackendUnpinnedForDefaultAffinity) {
  quill::BackendOptions backend_options;
  LogConfig log_config;
  backend_options.cpu_affinity = {1, 2};

  SetBackendCpuAffinity(backend_options, log_config.backend_cpu_affinity());

  EXPECT_TRUE(backend_options.cpu_affinity.empty());
}

TEST(LogBackendOptionsTest, PinsBackendToConfiguredCpu) {
  quill::BackendOptions backend_options;

  SetBackendCpuAffinity(backend_options, 7);

  ASSERT_EQ(backend_options.cpu_affinity.size(), 1U);
  EXPECT_EQ(backend_options.cpu_affinity.front(), 7U);
}

}  // namespace
}  // namespace nova::detail
