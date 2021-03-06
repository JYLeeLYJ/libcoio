#include "future.hpp"
#include "io_context.hpp"
#include "ioutils/tcp.hpp"
#include "ioutils/udp.hpp"
#include "time_delay.hpp"
#include <gtest/gtest.h>
#include <span>
#include <string_view>

using namespace std::literals;

static_assert(sizeof(coio::socket_base<coio::ipv4::udp>) ==
                  sizeof(coio::file_descriptor_base) +
                      sizeof(coio::ipv4::address),
              "empty protocol member should be optimized. ");

TEST(test_sock, test_address) {
  coio::ipv4::address addr_v4{2001, "127.0.0.1"};
  EXPECT_EQ(addr_v4.len(), sizeof(addr_v4.addr));
  EXPECT_EQ(addr_v4.to_string(), "127.0.0.1:2001");
}

TEST(test_sock, test_udp) {

  coio::ipv4::address s_addr{8888};

  auto ctx = coio::io_context{};
  auto _ = ctx.bind_this_thread();

  auto ptr = std::exception_ptr{};

  auto run_client = [&]() -> coio::future<void> {
    try {
      coio::udp_sock client{};
      client.bind(coio::ipv4::address{11451});

      auto sendbuf = "hello world."s;
      auto n = co_await client.sendto(s_addr, coio::to_bytes(sendbuf));
      EXPECT_EQ(n, 12);

    } catch (...) {
      ptr = std::current_exception();
      ctx.request_stop();
    }
  };

  auto run_server = [&]() -> coio::future<void> {
    try {
      coio::udp_sock server{};
      server.bind(s_addr);

      EXPECT_EQ(server.local_address(), coio::ipv4::address{8888});

      char recvbuf[20]{};
      coio::ipv4::address addr{};
      auto n = co_await server.recvfrom(addr, coio::to_bytes(recvbuf));
      auto expect_addr = coio::ipv4::address{11451, "127.0.0.1"};
      EXPECT_EQ(addr, expect_addr);
      EXPECT_EQ(n, 12);
      auto s = std::string_view{recvbuf, n};
      EXPECT_EQ(s, "hello world."sv);

    } catch (...) {
      ptr = std::current_exception();
    }

    ctx.request_stop();
  };

  ctx.co_spawn(run_server()); // suspend on : co_await server.recvfrom(...)
  ctx.co_spawn(run_client()); // suspend on : co_await client.sendto(...)
  ctx.run();
  if (ptr)
    std::rethrow_exception(ptr);
}

TEST(test_sock, test_tcp) {
  auto ctx = coio::io_context{};
  auto _ = ctx.bind_this_thread();

  auto s_addr = coio::ipv4::address{8889};
  char hello[] = {"hello world."};
  auto ptr = std::exception_ptr{};

  auto run_client = [&]() -> coio::future<void> {
    try {

      auto connector = coio::connector{};
      connector.bind(coio::ipv4::address{11454});
      co_await connector.connect(coio::ipv4::address{8889});

      auto &sock = connector.socket();
      auto n = co_await sock.send(std::as_bytes(std::span{hello}));
      EXPECT_EQ(n, 13);

    } catch (...) {
      ptr = std::current_exception();
      ctx.request_stop();
    }
  };

  auto run_server = [&]() -> coio::future<void> {
    try {

      auto acceptor = coio::acceptor{};
      acceptor.bind(s_addr);
      acceptor.listen();

      auto sock = co_await acceptor.accept();

      EXPECT_EQ(sock.get_peer_address(),
                (coio::ipv4::address{11454, "127.0.0.1"}));

      char buf[20]{};
      auto n = co_await sock.recv(std::as_writable_bytes(std::span{buf}));

      EXPECT_EQ(n, 13);
      EXPECT_EQ(std::string_view(buf, 13), std::string_view(hello, 13));

    } catch (...) {
      ptr = std::current_exception();
    }

    ctx.request_stop();
    co_return;
  };

  ctx.co_spawn(run_server()); // suspend on : co_await acceptor.accept()
  ctx.co_spawn(run_client()); // suspend on : co_await connector.connect(...)

  if (ptr)
    std::rethrow_exception(ptr);
  ctx.run();
  if (ptr)
    std::rethrow_exception(ptr);
}

TEST(test_sock, test_no_sigpipe) {
  auto ctx = coio::io_context{};
  auto _ = ctx.bind_this_thread();
  auto ptr = std::exception_ptr{};

  auto client = [&]() -> coio::future<void> {
    try {

      auto conn = coio::connector{};
      auto &sock = conn.socket();
      // throw std::system_error "Broken pipe"
      EXPECT_ANY_THROW(co_await sock.send(std::as_bytes(std::span{"114514"})));

    } catch (...) {
      ptr = std::current_exception();
    }

    ctx.request_stop();
  };

  ctx.co_spawn(client());
  ctx.run();
  if (ptr)
    std::rethrow_exception(ptr);
}

// TODO : test ipv6 / local socket
