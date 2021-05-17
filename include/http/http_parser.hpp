#ifndef COIO_HTTP_PARSER_HPP
#define COIO_HTTP_PARSER_HPP

#include "picohttpparser.h"
#include <algorithm>
#include <utility>
#include <array>
#include <cctype>
#include <ranges>
#include <string_view>
#include <vector>

namespace coio {

namespace details{
 
static bool is_lower_equal(std::string_view a, std::string_view b) {
  constexpr auto tolower = [](char c) { return std::tolower(c); };
  return std::ranges::equal(a | std::views::transform(tolower),
                            b | std::views::transform(tolower));
} 

}

class http_header{
  std::vector<std::pair<std::string , std::string>> m_headers;
public:
  explicit http_header() = default;

  template<std::ranges::sized_range R>
  requires std::same_as<phr_header , std::ranges::range_value_t<R>>
  explicit http_header(R && r) {
    m_headers.reserve(r.size());
    for(auto & h : r){
      m_headers.emplace_back(std::string{h.name , h.name_len} , std::string{h.value , h.value_len});
    }
  }

  // return nullopt if not contains this header
  // "" header value is allow
  std::optional<std::string_view> get_header_value(std::string_view key){
    for(auto & [k , v] : m_headers){
      if(details::is_lower_equal(k , key)) return v;
    }
    return {};
  }

  void add_headers(std::string k , std::string v){
    m_headers.emplace_back(std::move(k) , std::move(v));
  }
};

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
    auto content_len = get_header_value("content-length");
    if (!content_len.empty())
      body_len = std::stoi(std::string(content_len));

    return len;
  }

  std::string_view get_header_value(std::string_view key) {
    for (auto &h : headers | std::views::take(headers_cnt)) {
      if (details::is_lower_equal(std::string_view{h.name, h.name_len}, key)) {
        return {h.value, h.value_len};
      }
    }
    return {};
  }
};

} // namespace coio

#endif