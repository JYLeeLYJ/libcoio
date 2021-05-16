#include <gtest/gtest.h>
#include <string>

#include "common/result_type.hpp"
#include "stream_buffer.hpp"

using namespace std::literals;
using namespace coio;

// TODO : test result in constant evaluation context

TEST(test_common, test_result) {
  result<int, std::string> res = error{"shit error."};
  EXPECT_TRUE(res.is_error());
  EXPECT_EQ(res.get_error(), "shit error."s);

  bool get_res = false;
  auto r2 = res.map([&](int) { get_res = true; });

  // propagate
  EXPECT_EQ(r2.get_error(), "shit error.");
  EXPECT_FALSE(get_res);
}

TEST(test_common, test_stream_buffer) {
  auto buffer = stream_buffer{};
  EXPECT_EQ(buffer.data(), ""sv);
  EXPECT_EQ(buffer.size(), 0);

  auto wb = buffer.prepare(32);
  auto s = "hello"sv;
  memcpy(wb.data(), s.data(), s.size());
  EXPECT_FALSE(buffer.try_get_data(s.size()));

  buffer.commit(s.size());
  auto hello = buffer.try_get_data(s.size()).value();
  EXPECT_EQ(hello, s);
  buffer.consume(s.size());
  EXPECT_EQ(buffer.data(), ""sv);
}