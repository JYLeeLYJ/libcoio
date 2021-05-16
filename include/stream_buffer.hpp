#ifndef COIO_STREAMBUF_HPP
#define COIO_STREAMBUF_HPP

#include "buffer.hpp"
#include <streambuf>
#include <vector>

namespace coio {

class stream_buffer : public std::streambuf {

public:
  explicit stream_buffer() : m_buf(128) {
    auto p = m_buf.data();
    setp(p, p + m_buf.size());
    setg(p, p, p);
  }

  stream_buffer(stream_buffer &&) noexcept = default;
  stream_buffer(const stream_buffer &) = default;
  stream_buffer &operator=(stream_buffer &&) noexcept = default;
  stream_buffer &operator=(const stream_buffer &) = default;

  // streambuf structure :
  // [in:get area] eback , gptr , egptr <= [out:put area] pbase , pptr , epptr
public:
  void commit(std::size_t n) {
    pbump(std::min(n, static_cast<std::size_t>(epptr() - pptr())));
    setg(eback(), gptr(), pptr());
  }

  void consume(std::size_t n) {
    if (egptr() < pptr())
      setg(eback(), gptr(), pptr());
    gbump(std::min(n, static_cast<std::size_t>(egptr() - gptr())));
  }

  // void consume_all(){
  //     setg(eback() , pptr() , pptr());
  // }

  [[nodiscard]] auto prepare(std::size_t n) -> concepts::writeable_buffer auto {
    reserve(n);
    return std::as_writable_bytes(std::span{pptr(), n});
  }

  std::size_t size() const noexcept { return pptr() - gptr(); }

  std::string_view data() const noexcept {
    return {gptr(), static_cast<std::size_t>(pptr() - gptr())};
  }
  std::optional<std::string_view> try_get_data(std::size_t n) const noexcept {
    if (size() < n)
      return {};
    else
      return std::string_view{gptr(), gptr() + n};
  }

  void reserve(std::size_t n) {
    // rest put area is enough
    if (std::size_t res_n = epptr() - pptr(); res_n >= n)
      return;

    auto get_cur_pos = gptr() - m_buf.data();
    auto put_cur_pos = pptr() - m_buf.data();
    auto put_end_pos = put_cur_pos + n; // new end pos

    m_buf.resize(put_cur_pos + n);
    auto base = m_buf.data();
    setg(base, base + get_cur_pos, base + put_cur_pos);
    setp(base + put_cur_pos, base + put_end_pos);
  }

protected:
  // underflow : invoke when get area is empty
  std::streambuf::int_type underflow() override {
    if (gptr() < pptr()) {
      setg(eback(), gptr(), pptr());
      return traits_type::to_int_type(*gptr());
    } else
      return traits_type::eof();
  }

  // overflow : invoke when put ares is full
  std::streambuf::int_type overflow(int_type ch) override {
    if (traits_type::eq_int_type(ch, traits_type::eof())) {
      return traits_type::not_eof(ch);
    }

    reserve(64);
    *pptr() = traits_type::to_char_type(ch);
    pbump(1);
    return ch;
  }

private:
  std::vector<char> m_buf;
};

} // namespace coio

#endif