#ifndef COIO_SYNC_WAIT_IMPL_HPP
#define COIO_SYNC_WAIT_IMPL_HPP

#include <coroutine>
#include <atomic>
#include <exception>
#include <type_traits>
#include <version>

#include "details/callback_awaiter.hpp"
#include "common/non_copyable.hpp"
#include "awaitable.hpp"

namespace coio{

struct void_value{};

namespace details{

template<class T>
struct promise_base{
    using value_type = std::remove_reference_t<T>;
    auto yield_value( value_type && value) noexcept{
        m_pointer = std::addressof(value);
        return std::suspend_always{};
    }

    auto yield_value(value_type & value) noexcept{
        m_pointer = std::addressof(value);
        return std::suspend_always{};
    }

    template<class R , class Fn>
    auto yield_value(with_callback<R,Fn> && ret_with_cb){
        m_pointer = std::addressof(ret_with_cb.result);
        return final_callback_awaiter<Fn>{.callback = ret_with_cb.fn};
    }

    decltype(auto) get() {
        return static_cast<T&&>(*m_pointer);
    }
    value_type * m_pointer ;
};

template<>
struct promise_base<void>{
    constexpr void get() {}
};

template<class T , class CallbackT = void>
class awaitable_wrapper : non_copyable{
public:

    struct promise_type : promise_base<T>{

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

        void unhandled_exception() {
            m_exception = std::current_exception();
        }

        decltype(auto) result() {
            check_exception();    
            return this->get();
        }

        void check_exception(){
            if(m_exception)[[unlikely]]
                std::rethrow_exception(m_exception);
        }

        std::exception_ptr m_exception;
    };

    awaitable_wrapper(std::coroutine_handle<promise_type> handle) noexcept
    :m_handle(handle){}

    awaitable_wrapper(awaitable_wrapper && other) noexcept
    :m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    awaitable_wrapper& operator= (awaitable_wrapper&& other) noexcept {
        if(other.m_handle != m_handle) this->~awaitable_wrapper();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
        return *this;
    }

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