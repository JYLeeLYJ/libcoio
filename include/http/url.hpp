#ifndef COIO_URL_HPP
#define COIO_URL_HPP

#include "common/result_type.hpp"
#include <string_view>

namespace coio {

struct url {

  std::string_view proto;
  std::string_view host;
  std::string_view path;
  std::string_view query;

  enum url_err {
    invalid_proto,
    invalid_host,
    invalid_path,
    invalid_query,
  };

  static constexpr std::string_view to_string(url_err e) {
    using namespace std::literals;
    switch (e) {
    case url_err::invalid_proto:
      return "invalid_proto"sv;
    case url_err::invalid_host:
      return "invalid_host"sv;
    case url_err::invalid_path:
      return "invalid_path"sv;
    case url_err::invalid_query:
      return "invalid_query"sv;
    };
    return ""sv;
  }

  static constexpr auto parse(std::string_view url_str)
      -> result<url, url_err> {
    using namespace std::literals;

    auto sep_proto = "://"sv;
    auto pos = url_str.find(sep_proto);
    if (pos == std::string_view::npos) {
      return error(invalid_proto);
    }
    auto proto = url_str.substr(0, pos);
    url_str.remove_prefix(proto.size() + 3);

    pos = url_str.find('/');
    if (pos == std::string_view::npos) {
      return error(invalid_host);
    }
    auto host = url_str.substr(0, pos);
    url_str.remove_prefix(host.size());

    pos = url_str.find('?');
    if (pos == std::string_view::npos)
      return url{proto, host, url_str, ""sv};

    auto path = url_str.substr(0, pos);
    url_str.remove_prefix(path.size() + 1);
    auto query = url_str;

    return url{proto, host, path, query};
  }
};

} // namespace coio

#endif