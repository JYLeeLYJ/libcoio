#ifndef COIO_HTTP_PARSER_HPP
#define COIO_HTTP_PARSER_HPP

#include "picohttpparser.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <ranges>
#include <string_view>

namespace coio {

struct http_parser {
  static inline constexpr std::size_t num_headers = 100;
  std::array<phr_header, num_headers> headers{};
  std::size_t headers_cnt{};
  std::size_t body_len{};
  std::string_view message{};
  int status{};

  int parse_response(std::string_view str) {
    int min_ver{};
    const char *msg{};
    std::size_t msg_len{};
    headers_cnt = num_headers;
    auto len =
        phr_parse_response(str.data(), str.size(), &min_ver, &status, &msg,
                           &msg_len, headers.data(), &headers_cnt, 0);

    message = {msg, msg_len};
    auto content_len = get_header_value("content-lenght");
    if (!content_len.empty())
      body_len = std::stoi(std::string(content_len));

    return len;
  }

  std::string_view get_header_value(std::string_view key) {
    for (auto &h : headers | std::views::take(headers_cnt)) {
      if (is_equal(std::string_view{h.name, h.name_len}, key)) {
        return {h.value, h.value_len};
      }
    }
    return {};
  }

private:
  static bool is_equal(std::string_view a, std::string_view b) {
    auto tolower = [](char c) { return std::tolower(c); };
    return std::ranges::equal(a | std::views::transform(tolower),
                              b | std::views::transform(tolower));
  }
};

} // namespace coio

#endif