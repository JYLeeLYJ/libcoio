#ifndef COIO_SYNC_WAIT_IMPL_HPP
#define COIO_SYNC_WAIT_IMPL_HPP

#include <coroutine>
#include <atomic>
#include <exception>
#include <type_traits>
#include <version>

#include "common/non_copyable.hpp"
#include "awaitable.hpp"

namespace coio{

struct void_value{};

namespace details{

template<class T>
class awaitable_wrapper : non_copyable{
public:
    struct promise_type{
        using value_type = std::remove_reference_t<T>;

        constexpr auto initial_suspend() noexcept{
            return std::suspend_always{};
        }
        constexpr auto final_suspend() noexcept{
            return std::suspend_always{};
        }

        auto get_return_object() noexcept{
            return std::coroutine_handle<promise_type>::from_promise(*this);
        }

        void return_void() {}

        //U = T , and enable yield_value function if T is not void
        template<class U>
        requires (!std::is_void_v<U> && std::is_same_v<value_type,std::remove_reference_t<U>>)
        auto yield_value( U && value) noexcept {
            m_pointer = std::addressof(value);
            return std::suspend_always{};
        }

        void unhandled_exception() {
            m_exception = std::current_exception();
        }

        decltype(auto) result() {
            check_exception();    
            if constexpr(!std::is_void_v<T>)  
                return static_cast<T&&>(*m_pointer);
        }

        void check_exception(){
            if(m_exception)[[unlikely]]
                std::rethrow_exception(m_exception);
        }

        value_type * m_pointer;
        std::exception_ptr m_exception;
    };

    awaitable_wrapper(std::coroutine_handle<promise_type> handle) noexcept
    :m_handle(handle){}

    awaitable_wrapper(awaitable_wrapper && other) noexcept
    :m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    awaitable_wrapper& operator= (awaitable_wrapper&& other) noexcept = delete;

    ~awaitable_wrapper(){
        if(m_handle) m_handle.destroy();
    }

    void start(){
        if(m_handle) m_handle.resume();
    }

    bool is_ready() const noexcept{
        return !m_handle || m_handle.done();
    }

    decltype(auto) get(){
        return m_handle.promise().result();
    }

    decltype(auto) get_non_void(){
        if constexpr (std::is_void_v<T>)
            return void_value{};
        else 
            return m_handle.promise().result();
    }

    std::coroutine_handle<> handle() const{
        return m_handle;
    }

private:
    std::coroutine_handle<promise_type> m_handle;
};

}
}
#endif