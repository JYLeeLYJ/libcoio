#ifndef COIO_RESOLVER_HPP
#define COIO_RESOLVER_HPP

#include <coroutine>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#ifndef __cpp_lib_atomic_wait
// disable content_t usage to keep this lib header only
#define __NO_TABLE
// https://github.com/ogiroux/atomic_wait
#include "common/atomic_wait.hpp"
#endif
#include "awaitable.hpp"
#include "io_context.hpp"
#include "ioutils/protocol.hpp"
#include <ares.h>

namespace coio {

// TODO : wrap c-ares in io_uring
// TODO : support both ipv4 and ipv6

struct address_view {
  ares_addrinfo_cname *name;
  ares_addrinfo_node *node;

  std::string_view get_name() const noexcept { return name ? name->name : ""; }
  bool is_v4() const noexcept { return node->ai_family == AF_INET; }
  auto to_address() const {
    if (node->ai_addrlen != sizeof(sockaddr_in))
      throw std::logic_error("incorrect address.");
    auto paddr = reinterpret_cast<sockaddr_in *>(node->ai_addr);
    return ipv4::address::from_raw_address(*paddr);
  }
};

struct address_list {
  std::unique_ptr<ares_addrinfo, decltype(&::ares_freeaddrinfo)> head;

  struct iterator_type : private address_view {
    explicit iterator_type(ares_addrinfo_cname *pname,
                           ares_addrinfo_node *pnode) noexcept
        : address_view{pname, pnode} {}

    iterator_type &operator++() noexcept {
      name = name ? name->next : nullptr;
      node = node ? node->ai_next : nullptr;
      return *this;
    }
    auto operator++(int) noexcept {
      auto it = *this;
      return ++it;
    }

    bool operator==(const iterator_type &it) const noexcept {
      return node == it.node;
    }

    const address_view &operator*() const noexcept { return *this; }
  };

  auto begin() const noexcept {
    return iterator_type{head->cnames, head->nodes};
  }

  auto end() const noexcept { return iterator_type{nullptr, nullptr}; }
};

// currently usage : poll in a single thread

class async_resolver {
private:
  async_resolver() {
    ares_library_init(ARES_LIB_INIT_ALL);
    if (auto ret = ares_init(&m_channel); ret != ARES_SUCCESS) {
      throw std::runtime_error{"async_resolver init failed."};
    }
    init();
  }

  ~async_resolver() {
    m_thread.request_stop();
    try_wakeup();

    ares_destroy(m_channel);
    ares_library_cleanup();
  }

  struct query_info {
    const char *host, *service;
    void *arg;
  };

public:
  static async_resolver &instance() {
    static async_resolver resolver{};
    return resolver;
  }

public:
  struct async_query_awaiter : std::suspend_always {
    async_resolver &resolver;
    std::coroutine_handle<> handle{};
    std::string host{};
    std::string service{};
    coio::io_context *ctx;
    ares_addrinfo *result{};
    int status;

    void await_suspend(std::coroutine_handle<> h) {
      handle = h;
      {
        auto guard = std::lock_guard{resolver.m_mut};
        resolver.m_query_list.push_back(
            query_info{host.c_str(), service.c_str(), this});
      }
      resolver.try_wakeup();
    }

    std::optional<address_list> await_resume() noexcept {
      if (status == ARES_SUCCESS && result) {
        return address_list{.head = {result, &ares_freeaddrinfo}};
      } else
        return {};
    }
  };

  static void query_callback(void *arg, int status, int timeouts,
                             ares_addrinfo *res) {
    auto awaiter = reinterpret_cast<async_query_awaiter *>(arg);
    awaiter->ctx->post([=]() {
      awaiter->status = status;
      awaiter->result = res;
      if (awaiter->handle) [[likely]] {
        awaiter->handle.resume();
      }
    });
    instance().m_pending_cnt.fetch_sub(1, std::memory_order_relaxed);
  }

protected:
  void init() {
    m_thread = std::jthread([this](std::stop_token token) {
      int nfds;
      // int count;
      fd_set readers, writers;
      timeval tv, *tvp;
      std::vector<query_info> qlist;
      const auto def_hints = ares_addrinfo_hints{
          .ai_family = AF_INET,
      };

      while (!token.stop_requested()) {
        if (!m_query_list.empty()) {
          auto guard = std::lock_guard{m_mut};
          std::swap(qlist, m_query_list);
        }

        for (auto &q : qlist) {
          ares_getaddrinfo(m_channel, q.host, q.service, &def_hints,
                           query_callback, q.arg);
        }
        qlist.resize(0);

        FD_ZERO(&readers);
        FD_ZERO(&writers);

        nfds = ares_fds(m_channel, &readers, &writers);
        if (nfds == 0)
          break;
        tvp = ares_timeout(m_channel, NULL, &tv);
        select(nfds, &readers, &writers, NULL, tvp);
        ares_process(m_channel, &readers, &writers);

        // try blocking if finish all query
        if (m_pending_cnt.load(std::memory_order_relaxed) == 0) {
          std::atomic_wait(&m_pending_cnt, 0);
        }
      }
    });
  }

  void try_wakeup() {
    m_pending_cnt.fetch_add(1, std::memory_order_relaxed);
    std::atomic_notify_one(&m_pending_cnt);
  }

public:
  auto submit_query(std::string hostname, std::string service) {
    return async_query_awaiter{.resolver = *this,
                               .host = std::move(hostname),
                               .service = std::move(service),
                               .ctx = io_context::current_context()};
  }

private:
  ares_channel m_channel;
  std::jthread m_thread;
  std::mutex m_mut;
  std::vector<query_info> m_query_list;
  std::atomic<uint64_t> m_pending_cnt{0};
};

auto async_query(std::string hostname, std::string service)
    -> concepts::awaiter_of<std::optional<address_list>> auto {
  return async_resolver::instance().submit_query(std::move(hostname),
                                                 std::move(service));
}

} // namespace coio

#endif