#include "common/scope_guard.hpp"
#include "details/await_counter.hpp"
#include "future.hpp"
#include "http/httpclient.hpp"
#include "http/resolver.hpp"
#include "http/url.hpp"
#include "io_context.hpp"
#include <gtest/gtest.h>
#include <iostream>
#include <netdb.h>

using namespace std::literals;

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

TEST(test_http, test_url) {
  auto www = "https://en.cppreference.com/w/cpp/20"sv;
  auto result = coio::url::parse(www);
  // std::cout << "parse url : " <<  www << std::endl;
  if (result.is_error()) {
    std::cout << coio::url::to_string(result.get_error()) << std::endl;
    ASSERT_TRUE(false);
  }

  auto &u = result.value();

  // std::cout << "proto = " << u.proto << "\n"
  // << "host = "<< u.host << "\n"
  // << "path = "<< u.path << "\n"
  // << "query= "<< u.query << std::endl;

  EXPECT_EQ(u.proto, "https");
  EXPECT_EQ(u.host, "en.cppreference.com");
  EXPECT_EQ(u.path, "/w/cpp/20");
  EXPECT_EQ(u.query, "");

  result = coio::url::parse("http://purecpp.org/");
  ASSERT_TRUE(!result.is_error());
  EXPECT_EQ(result.value().proto, "http");
  EXPECT_EQ(result.value().host, "purecpp.org");
  EXPECT_EQ(result.value().path, "/");
  EXPECT_EQ(result.value().query, "");
}

TEST(test_http, test_header) {
  auto head = R"(HTTP/1.1 200 OK
Connection: close
Content-Type: text/html; charset=utf-8
Content-Length: 22305
Host: hahaha
Set-Cookie: CSESSIONID=aaaaaaaaaaaaaaaaaaaaaaaa; path=/
)"sv;

  coio::http_parser parser{};
  [[maybe_unused]] int len = parser.parse_response(head);
  auto content_len = parser.get_header_value("content-length");
  ASSERT_FALSE(content_len.empty());
  EXPECT_EQ(std::stoi(std::string(content_len)), 22305);
  EXPECT_EQ(parser.body_len, 22305);
  EXPECT_EQ(parser.headers_cnt, 5);
  // for(auto & h : parser.headers | std::views::take(parser.headers_cnt))
  //   std::cout << std::string_view{h.name , h.name_len} << std::endl;
  // std::cout << parser.status << " " << parser.message << std::endl;
}

TEST(test_http, test_client) {
  auto client = coio::http_client{};
  auto ptr = std::exception_ptr{};

  auto ctx = coio::io_context{};
  auto _ = ctx.bind_this_thread();

  auto get = [&]() -> coio::future<void> {
    try {
      auto resp_res = co_await client.get("http://purecpp.org/");
      EXPECT_FALSE(resp_res.is_error());

      if (resp_res.is_error()) {
        std::cout << "get http://purecpp.org error:" << resp_res.get_error()
                  << std::endl;
        ctx.request_stop();
        co_return;
      }

      resp_res.map([](coio::http_response rsp) {
        EXPECT_EQ(rsp.status_code, 200);
        EXPECT_EQ(rsp.body_length, rsp.rsp.size());
        // std::cout << "rsp = " << rsp.rsp << std::endl;
      });

    } catch (const std::exception &e) {
      ptr = std::current_exception();
    }
    ctx.request_stop();
  };

  ctx.co_spawn(get());
  ctx.run();
  if (ptr)
    std::rethrow_exception(ptr);
}
