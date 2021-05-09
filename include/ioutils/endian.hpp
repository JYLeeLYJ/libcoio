#ifndef COIO_ENDIAN_HPP
#define COIO_ENDIAN_HPP

#include <arpa/inet.h>
#include <bit>

namespace coio {

static inline uint16_t host_to_net(uint16_t x) { return htons(x); }

static inline uint32_t host_to_net(uint32_t x) { return htonl(x); }

static inline uint64_t host_to_net(uint64_t x) {
  if constexpr (std::endian::native == std::endian::big)
    return x;
  else {
    return (static_cast<uint64_t>(htonl(x)) << 32) + htonl(x >> 32);
  }
}

static inline uint16_t net_to_host(uint16_t x) { return ntohs(x); }

static inline uint32_t net_to_host(uint32_t x) { return ntohl(x); }

static inline uint64_t net_to_host(uint64_t x) {
  if constexpr (std::endian::native == std::endian::big)
    return x;
  else
    return (static_cast<uint64_t>(ntohl(x)) << 32) + ntohl(x >> 32);
}

} // namespace coio

#endif