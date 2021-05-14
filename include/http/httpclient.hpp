#ifndef COIO_HTTP_CLIENT_HPP
#define COIO_HTTP_CLIENT_HPP

#include <map>
#include <memory>
#include <string_view>

#include "common/result_type.hpp"
#include "common/scope_guard.hpp"
#include "future.hpp"
#include "http/http_parser.hpp"
#include "http/resolver.hpp"
#include "http/url.hpp"
#include "ioutils/tcp.hpp"
#include "stream_buffer.hpp"

namespace coio {

using namespace std::literals;

enum class http_method {
  get,
  post,
  head,
  // TODO : add more methods
};

static std::string_view to_str(http_method m) {
  switch (m) {
  case http_method::get:
    return "GET";
  case http_method::post:
    return "POST";
  case http_method::head:
    return "HEAD";
  default:
    return "UNKONWN";
  };
}

struct http_response {
  int status_code;
  std::string rsp{};
  // TODO : add headers field
};

// not thread safe
// TODO : support trunk , keep-alive
class http_client {
public:
  explicit http_client() : m_parser(std::make_unique<http_parser>()){};

public:
  auto get(std::string_view url) { return request(http_method::get, url, ""); }

  auto post(std::string_view url, std::string_view body) {
    return request(http_method::post, url, body);
  }

  // TODO : http request
  auto request(http_method method, std::string_view url, std::string_view body)
      -> future<result<http_response, std::string>> {

    if (method != http_method::post && !body.empty())
      co_return error("method error");

    // parse uri
    auto url_result = url::parse(url);
    if (url_result.is_error())
      co_return error("invalid url");
    auto &u = url_result.value();
    if (u.proto != "http") {
      // still not supported
      co_return error("unsupport protocal");
    }

    // query dns
    auto addr_list = co_await async_query(std::string(u.host).c_str(), "http");
    if (!addr_list)
      co_return error("invalid host name");

    // connect
    if (!co_await connect(addr_list.value()))
      co_return error("host connection failed");

    auto _ = scope_guard{[this]() { m_tcp_client.socket().close(); }};

    auto msg = build_msg(u, method, body);
    // do request write
    co_await m_tcp_client.socket().send(
        std::as_bytes(std::span{msg.data(), msg.size()}));

    // recv header
    if (!(co_await read_headers()))
      co_return error("http response parse error");

    // recv body
    http_response response{.status_code = m_parser->status};
    if (m_parser->body_len == 0)
      co_return response;
    response.rsp = co_await read_body(m_parser->body_len);
    co_return response;
  }

  void add_header(std::string k, std::string v) { m_headers[k] = v; }

private:
  future<bool> connect(address_list list) {
    for (auto &addr_view : list) {
      try {
        co_await m_tcp_client.connect(addr_view.to_address());
        co_return true;
      } catch (...) {
        continue;
      }
    }
    co_return false;
  }

  future<bool> read_headers() {
    auto head = co_await read_until(DCRLF);
    // read http head error
    if (head.empty() || !head.ends_with(DCRLF))
      co_return false;
    // parse head error if res < 0
    co_return m_parser->parse_response(head) > 0;
  }

  future<std::string> read_body(std::size_t need_read) {
    if (need_read == 0)
      co_return ""s;

    constexpr std::size_t min_buf_sz = 64;
    std::size_t n = 0;
    std::string rsp{};
    if (auto s = m_read_streambuf.try_get_data(need_read); s) {
      rsp.append(s);
      m_read_streambuf.consume(s.size());
      co_return rsp;
    }

    rsp.append(m_read_streambuf.data());
    m_read_streambuf.consume(rsp.size());
    need_read -= rsp.size();

    rsp.reserve(need_read);
    try {
      while (need_read > 0) {
        n = co_await m_tcp_client.recv(
            m_read_streambuf.prepare(std::max(need_read, min_buf_sz)));
        if (n == 0)
          break;
        if (n >= need_read)
          n = need_read;
        need_read -= n;
        m_read_streambuf.commit(n);
      }
    } catch (...) {
      rsp.append(m_read_streambuf.data());
      m_read_streambuf.consume(rsp.size());
    }
    co_return rsp;
  }

  future<std::string> read_until(std::string_view delim) {
    auto buf = m_read_streambuf.prepare(512);
    std::size_t n = 0;
    std::size_t beg = 0;
    std::string result{};

    while (n = co_await m_tcp_client.recv(buf), n != 0) {
      m_read_streambuf.commit(n);
      auto str = m_read_streambuf.data();
      auto pos = str.find(delim, beg);
      if (pos != std::string_view::npos) {
        pos += delim.size();
        result = std::string(str.substr(0, pos));
        m_read_streambuf.consume(pos);
        break;
      } else {
        beg = str.size() > delim.size() ? str.size() - delim.size() : 0;
      }
      buf = m_read_streambuf.prepare(256);
    }

    if (result.empty()) {
      result = m_read_streambuf.data();
      m_read_streambuf.consume(result.size());
    }
    co_return result;
  }

  std::string build_msg(const url &u, http_method method,
                        std::string_view body) {
    std::string write_msg{};
    write_msg.reserve(128);
    write_msg.append(to_str(method));
    write_msg.append(" ").append(u.path);
    if (!u.query.empty())
      write_msg.append("?").append(u.query);
    write_msg.append(" HTTP/1.1\r\nHost:").append(u.host).append(CRLF);

    for (auto &[k, v] : m_headers) {
      write_msg.append(k).append(": ").append(v).append(CRLF);
    }

    if (!body.empty() || method == http_method::post) {
      write_msg.append("Content-Length: ")
          .append(std::to_string(body.size()))
          .append(CRLF);
    }

    // default keep-alive
    if (!m_headers.contains("Connection")) {
      write_msg.append("Connection: keep-alive\r\n");
    }

    write_msg.append(CRLF);

    if (!body.empty()) {
      write_msg.append(body);
    }

    return write_msg;
  }

private:
  static inline constexpr auto CRLF = "\r\n"sv;
  static inline constexpr auto DCRLF = "\r\n\r\n"sv;

  // ipv4
  connector<> m_tcp_client;
  stream_buffer m_read_streambuf;
  std::unique_ptr<http_parser> m_parser;
  std::map<std::string, std::string, std::less<>> m_headers;
};

} // namespace coio

#endif