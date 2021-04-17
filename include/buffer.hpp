#ifndef COIO_BUFFER_HPP
#define COIO_BUFFER_HPP

#include <cstddef>
#include <ranges>
#include <concepts>
#include <array>
#include <vector>
#include <sys/uio.h>

namespace coio{

    namespace concepts{
        
        template<class T>
        concept buffer = requires (T t){
            {t.data()} -> std::convertible_to<const std::byte*>;
            {t.size()} -> std::same_as<std::size_t>;
        };

        template<class T>
        concept writeable_buffer = requires (T t){
            {t.data()} -> std::same_as<std::byte*> ;
            {t.size()} -> std::same_as<std::size_t>;
        };

        template<class S>
        concept writeable_buffer_sequence = 
            std::ranges::sized_range<S> && 
            writeable_buffer<std::ranges::range_value_t<S>>;

        template<class S>
        concept buffer_sequence = 
            std::ranges::sized_range<S> && 
            buffer<std::ranges::range_value_t<S>>;

    } // namespace concepts

    template<concepts::buffer ...T>
    auto make_iovecs(T &&... buff ){
        return std::array<::iovec , sizeof...(T)>{
            ::iovec{
                .iov_base = buff.data(),
                .iov_len = buff.size()
            }...,
        };
    }

    template<concepts::buffer_sequence T>
    auto make_iovecs(T && buffs){
        std::vector<::iovec> iovecs{};
        iovecs.reserve(buffs.size());
        for(auto & b : buffs) 
            iovecs.emplace_back(b.data() , b.size());
        return iovecs;
    }

}// namespace coio


#endif