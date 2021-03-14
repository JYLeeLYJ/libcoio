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
requires std::invocable<Fn , typename awaitable_traits<Awaitable>::await_resume_t>
struct fmap_awaitable : non_copyable{

    Awaitable awaitable;
    Fn continuation;

    fmap_awaitable(Awaitable && awaitable , Fn && fn) noexcept
    :awaitable(std::forward<Awaitable>(awaitable)) ,continuation(std::forward<Fn>(fn)) 
    {}

    fmap_awaitable(fmap_awaitable && other) = default;

    struct fmap_awaiter {
        //use rvalue
        using awaiter_t = typename awaitable_traits<Awaitable&>::awaiter_t;
        using await_result_t = typename awaitable_traits<Awaitable>::await_result_t;

        awaiter_t awaiter;
        Fn & fn;

        bool await_ready() noexcept {
            return awaiter.await_ready();
        }

        template<class P>
        decltype(auto) await_suspend(std::coroutine_handle<P> handle)
        noexcept(noexcept(awaiter.await_suspend(handle))) {
            return awaiter.await_suspend(handle);
        }

        decltype(auto) await_resume() {
            if constexpr (std::is_void_v<await_result_t>){
                awaiter.await_resume();
                return std::invoke(fn);
            }else{
                return std::invoke(fn , awaiter.await_resume());
            }
        }
    };

    auto operator co_await() {
        return fmap_awaiter{get_awaiter(awaitable) , continuation};
    }
};

// //deduction guide
// template<class A , class Fn>
// fmap_awaitable(A & , Fn &&) -> fmap_awaitable<A& , Fn>;

template<class Fn>
struct fmap_transform{
    Fn fn;
};

//then
template<class Fn>
auto fmap(Fn && fn) {
    return fmap_transform{std::forward<Fn>(fn)};
}

template<concepts::awaitable A , class Fn>
requires std::invocable<Fn , typename awaitable_traits<A>::await_resume_t>
auto fmap(A && awaitable , Fn && fn){
    return fmap_awaitable{std::forward<A>(awaitable) , std::forward<Fn>(fn)};
}

template<concepts::awaitable A , class Fn>
decltype(auto) operator | (A && awaitable , fmap_transform<Fn> && then){
    return fmap(std::forward<A>(awaitable) , std::move(then.fn));
}

}

#endif