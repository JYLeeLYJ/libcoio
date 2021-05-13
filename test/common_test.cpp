#include <gtest/gtest.h>
#include <string>

#include "common/result_type.hpp"

using namespace std::literals;

// TODO : test result in constant evaluation context

TEST(test_common, test_result) {
  using namespace coio;
  result<int, std::string> res = error{"shit error."};
  EXPECT_TRUE(res.is_error());
  EXPECT_EQ(res.get_error(), "shit error."s);

  bool get_res = false;
  auto r2 = res.map([&](int) { get_res = true; });

  // propagate
  EXPECT_EQ(r2.get_error(), "shit error.");
  EXPECT_FALSE(get_res);
}