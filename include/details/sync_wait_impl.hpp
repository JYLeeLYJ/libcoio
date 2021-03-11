#ifndef COIO_SYNC_WAIT_IMPL_HPP
#define COIO_SYNC_WAIT_IMPL_HPP

#include <coroutine>
#include <atomic>
#include <exception>
#include <type_traits>
#include <version>

#ifndef __cpp_lib_atomic_wait
//disable content_t usage to keep this lib header only
#define __NO_TABLE
// https://github.com/ogiroux/atomic_wait
#include "common/atomic_wait.hpp"
#endif
#include "common/non_copyable.hpp"
#include "awaitable.hpp"

namespace coio{

namespace details{

template<class T>
class sync_wait_wrapper : non_copyable{
public:
    struct promise_type{
        using value_type = std::remove_reference_t<T>;
        using result_pointer_t = value_type *;

        struct final_awaiter{
            constexpr bool await_ready() noexcept {return false;}
            void await_suspend(std::coroutine_handle<promise_type> handle){
                //notice finish
                auto & flag = handle.promise().m_finish;
                if(!flag.exchange(true , std::memory_order_acq_rel))
                    std::atomic_notify_all(&flag);
            }
            constexpr void await_resume() noexcept {}
        };

        //immediately execute
        constexpr auto initial_suspend() noexcept{
            return std::suspend_never{};
        }
        constexpr auto final_suspend() noexcept{
            return final_awaiter{};
        }

        auto get_return_object() noexcept{
            return std::coroutine_handle<promise_type>::from_promise(*this);
        }

        void return_void() {}

        auto yield_value( T && value) noexcept 
        requires (!std::same_as<void , T>){
            m_pointer = std::addressof(value);
            return final_awaiter{};
        }

        void unhandled_exception() {
            m_exception = std::current_exception();
        }

        T && result()
        requires (!std::same_as<void ,T>) {
            check_exception();          
            return static_cast<T&&>(*m_pointer);
        }

        void result(){
            check_exception();
        }

        void check_exception(){
            if(m_exception)
                std::rethrow_exception(m_exception);
        }

        std::atomic<bool> m_finish;
        result_pointer_t m_pointer;
        std::exception_ptr m_exception;
    };

    sync_wait_wrapper(std::coroutine_handle<promise_type> handle) noexcept
    :m_handle(handle){}

    sync_wait_wrapper(sync_wait_wrapper && other) noexcept = delete;
    sync_wait_wrapper& operator= (sync_wait_wrapper&& other) noexcept = delete;

    ~sync_wait_wrapper(){
        if(m_handle) m_handle.destroy();
    }

    decltype(auto) wait(){
        auto & flag = m_handle.promise().m_finish;
        std::atomic_wait_explicit(&flag ,false, std::memory_order_acquire );
        return m_handle.promise().result();
    }

private:
    std::coroutine_handle<promise_type> m_handle;
};

}
}
#endif