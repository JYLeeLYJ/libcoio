#ifndef COIO_FMAP_HPP
#define COIO_FMAP_HPP

#include "common/non_copyable.hpp"
#include "awaitable.hpp"

namespace coio{

//1. pipe operator | 
//2. fmap()
//3. fmap_transform
//4. fmap_awaitable

template<concepts::awaitable Awaitable , class Fn>
struct fmap_awaitable {

    Awaitable awaitable;
    Fn continuation;

    template<class A , class F>
    struct fmap_awaiter {
        //use lvalue
        using awaiter_t = typename awaitable_traits<A>::awaiter_t;

        awaiter_t awaiter;
        F & fn;

        bool await_ready() noexcept {
            return awaiter.await_ready();
        }

        template<class P>
        decltype(auto) await_suspend(std::coroutine_handle<P> handle)
        noexcept(noexcept(awaiter.await_suspend(handle))) {
            return awaiter.await_suspend(handle);
        }

        decltype(auto) await_resume() {
            if constexpr (std::is_void_v<decltype(awaiter.await_resume())>){
                awaiter.await_resume();
                return std::invoke(fn);
            }else{
                return std::invoke(fn , awaiter.await_resume());
            }
        }
    };

    auto operator co_await() {
        return fmap_awaiter<Awaitable&& , Fn>{
            .awaiter= get_awaiter(std::forward<Awaitable>(awaitable)) , 
            .fn     = continuation
        };
    }

};

template<class Fn>
struct fmap_transform{
    Fn fn;
};

//then
template<class Fn>
auto fmap(Fn && fn) {
    return fmap_transform<Fn>{std::forward<Fn>(fn)};
}

namespace concepts {
    template<class F , class Arg>
    concept transform_function = 
        ( std::is_void_v<Arg> && std::invocable<F> ) 
        ||std::invocable<F , Arg> ;

}

template<concepts::awaitable A , class Fn>
// requires concepts::transform_function<Fn ,typename awaitable_traits<A>::await_resume_t>
auto fmap( A && awaitable ,Fn && fn ){
    return fmap_awaitable<A , Fn>{
        .awaitable = std::forward<A>(awaitable) , 
        .continuation = std::forward<Fn>(fn)
    };
}

template<class A , class Fn>
decltype(auto) operator | (A && awaitable , fmap_transform<Fn> & then){
    return fmap(std::forward<A>(awaitable) ,then.fn );
}

template<class A, class Fn>
decltype(auto) operator | (A && awaitable , fmap_transform<Fn> && then){
    return fmap( std::forward<A>(awaitable) , std::forward<decltype(then)>(then).fn);
}

}

#endif