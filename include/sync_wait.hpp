#ifndef COIO_SYNC_WAIT_HPP
#define COIO_SYNC_WAIT_HPP

#include "details/sync_wait_impl.hpp"

namespace coio{

template<concepts::awaitable F , typename R = awaitable_traits<F>::await_result_t>
details::sync_wait_wrapper<R> make_sync_wait(F && f) {
    co_yield co_await std::forward<F>(f);
}

template<concepts::awaitable F , typename R = awaitable_traits<F>::await_result_t>
requires std::same_as<void ,R>
details::sync_wait_wrapper<void> make_sync_wait(F && f) {
    co_await std::forward<F>(f);
}

template<concepts::awaitable F>
auto sync_wait(F && f){
    auto task = make_sync_wait(std::forward<F>(f));
    return task.wait();
}

}

#endif