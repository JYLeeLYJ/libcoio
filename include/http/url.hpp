#ifndef COIO_URL_HPP
#ifndef COIO_URL_HPP

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

  static constexpr auto parse(std::string_view url_str)
      -> result<url, url_err> {
    using namespace std::literals;

    auto sep_proto = "://"sv;
    auto pos = url_str.find(sep_proto);
    if (pos == std::string_view::npos) {
      return error(invalid_proto);
    }
    auto proto = url_str.substr(0, pos);
    url_str.remove_prefix(pos + 3);

    pos = url_str.find('/');
    if (pos == std::string_view::npos) {
      return error(invalid_host);
    }
    auto host = url_str.substr(pos);
    url_str.remove_prefix(pos + 1);

    pos = url_str.find('?');
    if (pos == std::string_view::npos) {
      return error(invalid_path);
    }
    auto path = url_str.substr(pos);
    url_str.remove_prefix(pos + 1);
    auto query = url_str;

    return url{proto, host, path, query};
  }
};

} // namespace coio

#endif