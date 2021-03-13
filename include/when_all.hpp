#ifndef COIO_WHEN_ALL_HPP
#define COIO_WHEN_ALL_HPP

#include <vector>
#include <tuple>

#include "future.hpp"
#include "details/await_counter.hpp"
#include "details/awaitable_wrapper.hpp"
#include "details/callback_awaiter.hpp"

namespace coio{

template<concepts::awaitable A>
requires concepts::void_type<typename awaitable_traits<A>::await_resume_t>
auto make_when_all_wait_task(A & a , awaitable_counter & counter )
-> details::awaitable_wrapper<void> {
    co_await details::attach_callback(a ,  [&]()noexcept{counter.notify_complete_one();});
}

template<concepts::awaitable A>
auto make_when_all_wait_task(A & a , awaitable_counter & counter)
-> details::awaitable_wrapper<typename awaitable_traits<A>::await_resume_t>{
    co_yield co_await details::attach_callback(a ,  [&]()noexcept{counter.notify_complete_one();});
}

template<concepts::awaitable A , class R = awaitable_traits<A>::await_result_t > 
requires concepts::void_type<R>
future<void> when_all(std::vector<A> awaitables){

    using task_t=  details::awaitable_wrapper<void>;
    awaitable_counter counter{.cnt = awaitables.size() };
    std::vector<task_t> v{};
    for(auto & a : awaitables){
        auto task = make_when_all_wait_task(a , counter);
        task.start();
        v.emplace_back(std::move(task));
    }
    
    co_await counter;
}

template<concepts::awaitable A , class R = awaitable_traits<A>::await_result_t >
future<std::vector<R>> when_all(std::vector<A> awaitables){
    using task_t = details::awaitable_wrapper<R>;

    awaitable_counter counter{.cnt = awaitables.size()};
    std::vector<task_t> tasks{};
    for(auto & a : awaitables){
        auto task = make_when_all_wait_task(a , counter);
        task.start();
        tasks.emplace_back(std::move(task));
    }

    co_await counter;
    
    std::vector<R> results{};
    for(auto & t : tasks){
        results.emplace_back(std::move(t.get()));
    }
    co_return std::move(results);
}


}

#endif