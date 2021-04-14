#ifndef COIO_FUTURE_HPP
#define COIO_FUTURE_HPP

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <concepts>
#include <variant>
#include <atomic>
#include <cassert>

#include "common/non_copyable.hpp"
#include "common/overloaded.hpp"
#include "common/type_concepts.hpp"

namespace coio{
    
    class future_promise_base{
    public:
        future_promise_base() noexcept = default;
    public:
        struct final_awaiter{
            bool await_ready() const noexcept {return false;}

            template<class Promise>
            void await_suspend(std::coroutine_handle<Promise> handle){
                auto & promise = handle.promise();
                if(promise.m_state.exchange(true , std::memory_order_acq_rel)){
                    promise.m_continuation.resume();
                }
            }

            void await_resume() noexcept{}

        };

        constexpr auto initial_suspend() noexcept{
            return std::suspend_always{};
        }

        constexpr auto final_suspend() noexcept{
            return final_awaiter{};
        }

        bool set_continuation(std::coroutine_handle<> continuation){
            m_continuation = continuation;
            return !m_state.exchange(true , std::memory_order_acq_rel);
        }

    private:
        std::atomic<bool> m_state{false};
        std::coroutine_handle<> m_continuation;
    };

    namespace concepts{
        
    template<class T>
    concept future_value =
        concepts::value_type<T> || 
        concepts::void_type<T> ;//||
        // concepts::lvalue_reference<T>;
    }

    template<concepts::future_value T>
    class future;

    template<class T>
    class future_promise : public future_promise_base{
    public:
        void unhandled_exception(){
            m_result = std::current_exception();
        }

        template<class Value>
        requires std::constructible_from<T , Value>
        void return_value(Value && v){
            m_result.template emplace<T>(std::forward<Value>(v));
        }

        auto get_return_object() noexcept{
            return std::coroutine_handle<future_promise>::from_promise(*this);
        }

    public:
        T & result() & {
            return get();
        }

        T result() && requires std::is_scalar_v<T>{
            return get();
        }

        T&& result() && {
            return std::move(get());
        }
    private:
        bool has_value() const noexcept {
            return std::holds_alternative<T>(m_result);
        }

        T & get(){
            if(std::holds_alternative<std::exception_ptr>(m_result))
                std::rethrow_exception(*std::get_if<std::exception_ptr>(&m_result));
            assert(has_value());
            return *std::get_if<T>(&m_result);
        }
    private:
        std::variant<std::monostate ,T , std::exception_ptr> m_result{};
    };

    template<>
    class future_promise<void> : public future_promise_base{
    public:
        future_promise() noexcept = default;

        auto get_return_object() noexcept{
            return std::coroutine_handle<future_promise>::from_promise(*this);
        }

        void return_void() noexcept{}

        void result() {
            if(m_exception) [[unlikely]]
                std::rethrow_exception(m_exception);
        }

        void unhandled_exception() noexcept{
            m_exception = std::current_exception();
        }
    private:
        std::exception_ptr m_exception{};
    };

    template<concepts::future_value T>
    class future : non_copyable{
    public:
        using promise_type = future_promise<T>;
        using value_type = T;

        struct basic_awaiter{
            basic_awaiter(std::coroutine_handle<promise_type> handle) noexcept
            :m_handle(handle){}

            bool await_ready() const noexcept {
                return !m_handle || m_handle.done();
            }

            bool await_suspend(std::coroutine_handle<> continuation) noexcept{
                assert(m_handle);
                m_handle.resume();
                return m_handle.promise().set_continuation(continuation);
            }
            std::coroutine_handle<promise_type> m_handle;
        };

    public:

        future(std::coroutine_handle<promise_type> handle) noexcept
        : m_handle(handle) {}

        ~future() noexcept {
            if(m_handle) m_handle.destroy();
            m_handle = nullptr;
        }

        future(future && f) noexcept
        :m_handle(f.m_handle){
            f.m_handle = nullptr;
        }

        future& operator=(future && other) noexcept{
            this->~future();
            swap(m_handle , other.m_handle);
        }

    public:
    
        template<bool use_rvalue>
        struct awaiter : basic_awaiter{

            using basic_awaiter::basic_awaiter;

            decltype(auto) await_resume(){
                assert(this->m_handle);
                if constexpr (use_rvalue)
                    return std::move(this->m_handle.promise()).result();
                else 
                    return this->m_handle.promise().result();
            }
        };

        auto operator co_await() const && noexcept{
            return awaiter<true>{m_handle};
        }

        auto operator co_await() const & noexcept {
            return awaiter<false>{m_handle};
        }

    protected:

        bool is_ready() const noexcept{
            return !m_handle || m_handle.done();
        }

        auto await_when_ready() const noexcept{
            struct awaiter:basic_awaiter{
                using basic_awaiter::basic_awaiter;
                void await_resume() noexcept {}
            };
            return awaiter{m_handle};
        }

    private:
        std::coroutine_handle<promise_type> m_handle;
    };
}

#endif