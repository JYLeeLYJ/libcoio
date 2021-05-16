#include "common/scope_guard.hpp"
#include "details/await_counter.hpp"
#include "future.hpp"
#include "http/resolver.hpp"
#include "io_context.hpp"
#include <gtest/gtest.h>
#include <iostream>
#include <netdb.h>

// TODO : test resolver
TEST(test_http, test_resovler) {
  auto ctx = coio::io_context{};
  auto _ = ctx.bind_this_thread();
  auto counter = coio::awaitable_counter{.cnt = 2};
  ctx.co_spawn([&]() -> coio::future<void> {
    auto g =
        coio::scope_guard{[&]() noexcept { counter.notify_complete_one(); }};
    addrinfo *info;
    ::getaddrinfo("127.0.0.1", "8888", nullptr, &info);
    auto addr_list = co_await coio::async_query("127.0.0.1", "8888");
    EXPECT_TRUE(info);
    EXPECT_TRUE(addr_list);
    if (!addr_list)
      co_return;

    auto &list = addr_list.value();
    // std::cout << "resolver query 127.0.0.1 , 8888 : \n" ;
    EXPECT_TRUE(list.head);
    for (auto &l : list) {
      EXPECT_TRUE(l.node);
      auto addr = l.to_address();
      EXPECT_EQ(addr.addr.sin_family, AF_INET);
      std::cout << l.get_name() << " " << addr.to_string() << std::endl;
    }
  }());

  ctx.co_spawn([&]() -> coio::future<void> {
    auto g =
        coio::scope_guard{[&]() noexcept { counter.notify_complete_one(); }};
    auto list = co_await coio::async_query("cn.bing.com", "http");
    EXPECT_TRUE(list);
    if (!list)
      co_return;
    for (auto &l : list.value()) {
      EXPECT_TRUE(l.node);
      auto addr = l.to_address();
      EXPECT_EQ(addr.addr.sin_family, AF_INET);
      std::cout << l.get_name() << " " << addr.to_string() << std::endl;
    }
  }());

  auto fin = [&]() -> coio::future<void> {
    co_await counter;
    ctx.request_stop();
  };

  ctx.co_spawn(fin());
  ctx.run();
}

// TODO : url parser
// TEST()

// TODO : http parser

// TODO : http client