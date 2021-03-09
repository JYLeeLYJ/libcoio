#ifndef COIO_NONCOPYABLE_HPP
#define COIO_NONCOPYABLE_HPP

namespace coio{
    struct non_copyable{
        constexpr non_copyable() noexcept = default;
        non_copyable(const non_copyable&) = delete;
        non_copyable & operator=(const non_copyable &) = delete;
    };
}
#endif