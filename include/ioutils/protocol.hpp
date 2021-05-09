#ifndef COIO_NET_PROTOCOL_HPP
#define COIO_NET_PROTOCOL_HPP

#include <arpa/inet.h>
#include <sys/un.h>

#include "endian.hpp"

namespace coio {

namespace concepts {

template <class T>
concept ip_address = requires(T t) {
  {t.ptr()};
  {t.len()};
  {t == std::declval<T>()}; // Eq
};

template <class T>
concept protocal = requires(T t) {
  {t.domain()};
  {t.type()};
  ip_address<typename T::address_type>; // help for code completion
};
} // namespace concepts

namespace details {

struct tcp_type {
  constexpr auto type() const { return SOCK_STREAM; }
};

struct udp_type {
  constexpr auto type() const { return SOCK_DGRAM; }
};

// network to host

// host to network
} // namespace details

struct ipv4 {

  struct ip_domain {
    constexpr auto domain() const { return AF_INET; }
  };

  struct address {

    sockaddr_in addr{.sin_family = AF_INET};
    explicit address() noexcept = default;
    explicit address(uint16_t port, const char *ip = nullptr) noexcept {
      addr.sin_port = host_to_net(port);
      if (ip)
        ::inet_pton(ip_domain{}.domain(), ip, &addr.sin_addr);
      else
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    sockaddr *ptr() { return reinterpret_cast<sockaddr *>(&addr); }
    const sockaddr *ptr() const {
      return reinterpret_cast<const sockaddr *>(&addr);
    }
    constexpr socklen_t len() const { return sizeof(sockaddr_in); }

    constexpr bool operator==(const address &other) const noexcept {
      return addr.sin_family == other.addr.sin_family &&
             addr.sin_addr.s_addr == other.addr.sin_addr.s_addr &&
             addr.sin_port == other.addr.sin_port;
    }

    std::string to_string() const {
      char buf[INET_ADDRSTRLEN]{};
      ::inet_ntop(ip_domain{}.domain(), &addr.sin_addr, buf, INET_ADDRSTRLEN);
      return std::string{buf}.append(":") +
             std::to_string(net_to_host(addr.sin_port));
    }
  };

  struct tcp : ip_domain, details::tcp_type {
    using address_type = address;
  };

  struct udp : ip_domain, details::udp_type {
    using address_type = address;
  };

}; // namespace ipv4

struct ipv6 {

  struct ip_domain {
    constexpr auto domain() const { return AF_INET6; }
  };

  struct address {
    sockaddr_in6 addr{.sin6_family = AF_INET6};
    explicit address() noexcept = default;
    explicit address(uint16_t port, const char *ip = nullptr) noexcept {
      addr.sin6_port = host_to_net(port);
      if (ip)
        ::inet_pton(ip_domain{}.domain(), ip, &addr.sin6_addr);
      else
        addr.sin6_addr = IN6ADDR_ANY_INIT;
    }
    sockaddr *ptr() { return reinterpret_cast<sockaddr *>(&addr); }
    const sockaddr *ptr() const {
      return reinterpret_cast<const sockaddr *>(&addr);
    }
    constexpr socklen_t len() const { return sizeof(sockaddr_in6); }

    constexpr bool operator==(const address &other) const noexcept = default;

    std::string to_string() const {
      static thread_local char buf[INET6_ADDRSTRLEN]{};
      inet_ntop(ip_domain{}.domain(), &addr.sin6_addr, buf, INET6_ADDRSTRLEN);
      return std::string{buf}.append(":") +
             std::to_string(net_to_host(addr.sin6_port));
    }
  };

  struct tcp : ip_domain, details::tcp_type {
    using address_type = address;
  };

  struct udp : ip_domain, details::udp_type {
    using address_type = address;
  };

}; // namespace ipv6

struct iplocal {

  struct ip_domain {
    constexpr auto domain() const { return AF_LOCAL; }
  };

  struct address {
    sockaddr_un addr{.sun_family = AF_LOCAL}; // TODO : too large on stack
    explicit address() noexcept = default;
    explicit address(const char *path) noexcept {
      strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    }
    sockaddr *ptr() { return reinterpret_cast<sockaddr *>(&addr); }
    const sockaddr *ptr() const {
      return reinterpret_cast<const sockaddr *>(&addr);
    }
    constexpr socklen_t len() const { return sizeof(sockaddr_in); }

    constexpr bool operator==(const address &other) const noexcept {
      return strcmp(addr.sun_path, other.addr.sun_path) == 0;
    }
    std::string to_string() const { return std::string{addr.sun_path}; }
  };

  struct tcp : ip_domain, details::tcp_type {
    using address_type = address;
  };

  struct udp : ip_domain, details::udp_type {
    using address_type = address;
  };

}; // namespace iplocal

} // namespace coio

#endif