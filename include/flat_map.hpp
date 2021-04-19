#ifndef COIO_FLATMAP_HPP
#define COIO_FLATMAP_HPP

#include "awaitable.hpp"
#include "future.hpp"

namespace coio{

namespace concepts{
    template<class Fn , class ...Arg>
    concept binder_function = requires (Fn && f , Arg && ...arg){
        {std::invoke(std::forward<Fn>(f) , std::forward<Arg>(arg)...)} -> awaitable;
    };
}

//pipe operator 
//flat_map awaitable
//flat_map
//flat_map transform
namespace details{

    template<class Fn , class R>
    using flatmap_deduce_t = typename awaitable_traits<std::invoke_result_t<Fn,R>>::await_result_t;

    template<class Fn >
    using flatmap_deduce_void_t = typename awaitable_traits<std::invoke_result_t<Fn>>::await_result_t;

    //NOTE: use Awaitable instead of Awaitable && , 
    //cause this coroutine must own rvalue awaitable object
    //
    //awaitable A -> (void -> awaitable B) -> awaitable B
    template<concepts::awaitable Awaitable , class Fn >
    requires concepts::binder_function<Fn>
    auto flat_map_impl(Awaitable awaitable , Fn && fn) -> future<details::flatmap_deduce_void_t<Fn>>{
        //GCC BUG TRACK : https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99575
        // https://godbolt.org/z/anWWjb4j4
        // (void)co_await std::forward<Awaitable>(awaitable) ;
        (void) co_await reinterpret_cast<Awaitable&&>(awaitable);
        co_return co_await std::forward<Fn>(fn)();
    }

    //awaitable A -> (A -> awaitable B) -> awaitable B
    template<concepts::awaitable Awaitable , class Fn , class R = typename awaitable_traits<Awaitable>::await_resume_t>
    requires concepts::binder_function<Fn , R> 
    auto flat_map_impl(Awaitable awaitable , Fn && fn) -> future<details::flatmap_deduce_t<Fn , R>>{
        //GCC BUG TRACK : https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99575
        // https://godbolt.org/z/anWWjb4j4
        // co_return co_await std::forward<Fn>(fn)(co_await std::forward<Awaitable>(awaitable));
        co_return co_await std::forward<Fn>(fn)(co_await reinterpret_cast<Awaitable&&>(awaitable));
    }
}



template<class Fn>
struct flat_map_transform{
    Fn fn;
};

template<class Fn>
auto flat_map(Fn && fn){
    return flat_map_transform<Fn>{std::forward<Fn>(fn)};
}

//returns a future
template<concepts::awaitable Awaitable , class Fn >
auto flat_map(Awaitable && a , Fn && fn){
    return details::flat_map_impl<Awaitable>(std::forward<Awaitable>(a) , std::forward<Fn>(fn));
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