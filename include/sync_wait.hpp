#ifndef COIO_SYNC_WAIT_HPP
#define COIO_SYNC_WAIT_HPP

#ifndef __cpp_lib_atomic_wait
//disable content_t usage to keep this lib header only
#define __NO_TABLE
// https://github.com/ogiroux/atomic_wait
#include "common/atomic_wait.hpp"
#endif

#include "details/task_with_callback.hpp"

namespace coio{

//TODO : complete when exception

template<concepts::awaitable A , typename R = awaitable_traits<A>::await_resume_t>
details::task_with_callback<R> make_sync_wait(A && awaitable , std::atomic<bool> & flag) {
    // GCC BUG TRACK : https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99575
    // https://godbolt.org/z/anWWjb4j4
    // co_await std::forward<A>(awaitable)
    co_yield co_await reinterpret_cast<A&&>(awaitable) ;       
}

template<concepts::awaitable A , typename R = awaitable_traits<A>::await_resume_t>
requires std::same_as<void ,R>
details::task_with_callback<void> make_sync_wait(A && awaitable ,std::atomic<bool> & flag) {
    // GCC BUG TRACK : https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99575
    // https://godbolt.org/z/anWWjb4j4
    // co_await std::forward<A>(awaitable)
    co_await reinterpret_cast<A&&>(awaitable) ;
}

template<concepts::awaitable F>
decltype(auto) sync_wait(F && f){
    std::atomic<bool> flag{false};
    auto task = make_sync_wait(std::forward<F>(f) , flag);
    auto notice_fin = [&](){
        if(!flag.exchange(true , std::memory_order_acq_rel))
            std::atomic_notify_all(&flag);
    };
    task.set_callback(notice_fin);
    task.start();
    std::atomic_wait_explicit(&flag , false , std::memory_order_acquire );
    return task.get();
}

}

#endif