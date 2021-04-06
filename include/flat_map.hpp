#ifndef COIO_FLATMAP_HPP
#define COIO_FLATMAP_HPP

#include "awaitable.hpp"
#include "future.hpp"

namespace coio{

//pipe operator 
//flat_map awaitable
//flat_map
//flat_map transform
namespace details{

    template<class Fn , class R>
    using flatmap_deduce_t = typename awaitable_traits<std::invoke_result_t<Fn,R>>::await_result_t;

    template<class Fn >
    using flatmap_deduce_void_t = typename awaitable_traits<std::invoke_result_t<Fn>>::await_result_t;
}

//awaitable A -> (void -> awaitable B) -> awaitable B
template<concepts::awaitable Awaitable , class Fn >
requires concepts::coroutine<Fn>
auto flat_map(Awaitable && awaitable , Fn && fn) -> future<details::flatmap_deduce_void_t<Fn>>{

    co_await std::forward<Awaitable>(awaitable) ;
    co_return co_await std::forward<Fn>(fn)();
}

//awaitable A -> (A -> awaitable B) -> awaitable B
template<concepts::awaitable Awaitable , class Fn , class R = typename awaitable_traits<Awaitable>::await_resume_t>
requires concepts::coroutine<Fn , R> 
auto flat_map(Awaitable && awaitable , Fn && fn) -> future<details::flatmap_deduce_t<Fn , R>>{

    co_return co_await std::forward<Fn>(fn)(co_await std::forward<Awaitable>(awaitable));
}

template<class Fn>
struct flat_map_transform{
    Fn fn;
};

template<class Fn>
auto flat_map(Fn && fn){
    return flat_map_transform<Fn>{std::forward<Fn>(fn)};
}

template<concepts::awaitable A , class Fn>
auto operator | (A && a , flat_map_transform<Fn> && transform){
    return flat_map(std::forward<A>(a) , std::move(transform.fn));
}

template<concepts::awaitable A , class Fn>
auto operator | (A && a , flat_map_transform<Fn> & transform){
    return flat_map(std::forward<A>(a) , transform.fn);
}

}


#endif