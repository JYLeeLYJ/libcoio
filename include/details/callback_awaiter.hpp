#ifndef COIO_CALLBACK_AWAITER_HPP
#define COIO_CALLBACK_AWAITER_HPP

#include "awaitable.hpp"

namespace coio{

namespace details{

template<concepts::awaitable A , class Fn>
requires std::is_nothrow_invocable_r_v<void , Fn>
struct callback_awaiter{
    using awaiter_t = typename awaitable_traits<A&&>::awaiter_t;
    Fn && m_callback;
    awaiter_t m_awaiter;

    callback_awaiter(A && awaitable , Fn && callback_fn) 
    :m_callback(std::forward<Fn>(callback_fn)) 
    ,m_awaiter(get_awaiter(std::forward<A>(awaitable))) {}

    bool await_ready() noexcept{
        return static_cast<awaiter_t &&>(m_awaiter).await_ready();
    }

    template<class P>
    decltype(auto) await_suspend(std::coroutine_handle<P> handle) 
    noexcept (noexcept(static_cast<awaiter_t &&>(m_awaiter).await_suspend(handle))){
        return static_cast<awaiter_t &&>(m_awaiter).await_suspend(handle);
    }

    decltype(auto) await_resume()
    noexcept(noexcept(static_cast<awaiter_t&&>(m_awaiter).await_resume())){
        if constexpr (std::is_void_v<typename awaitable_traits<A&&>::await_result_t>){
            static_cast<awaiter_t&&>(m_awaiter).await_resume();
            m_callback();
        }
        else {
            decltype(auto) result = static_cast<awaiter_t&&>(m_awaiter).await_resume();
            m_callback();
            return result;
        }
    }
};

template<class Awaitable , class Fn>
auto attach_callback(Awaitable && awaitable , Fn && fn){
    return callback_awaiter<Awaitable && , Fn &&>{
        std::forward<Awaitable>(awaitable) , 
        std::forward<Fn>(fn)};
}

}

}

#endif
