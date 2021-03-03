#ifndef COIO_GENERATOR_HPP
#define COIO_GENERATOR_HPP

#include <coroutine>
#include <type_traits>
#include <exception>
#include <concepts>

namespace coio{

    struct non_copyable{
        non_copyable(const non_copyable&) = delete;
        non_copyable & operator=(const non_copyable &) = delete;
    };

    template<class T>
    class generator:private non_copyable{
    public:
        class promise;
        using value_type    = std::remove_reference_t<T>;
        using reference_type= std::conditional_t<std::is_reference_v<T>, T , T&>;
        using promise_type = promise;

        class promise{
        public:
            promise() = default;
        public:
            constexpr auto initial_suspend() const noexcept{
                return std::suspend_always{};
            }
            constexpr auto final_suspend() const noexcept{
                return std::suspend_always{};
            }

            template<std::same_as<value_type> Value>
            std::suspend_always yield_value(Value && value) noexcept{
                m_value_ptr = std::addressof(value);
                return {};
            }

            generator<T> get_return_object() noexcept{
                return generator<T>{ std::coroutine_handle<promise<T>>::from_promise(*this)};
            }
            
            void return_void(){}

            //disable co_await
            template<class U>
            std::suspend_never await_transform(U && value) = delete;

        public:
            reference_type value() const noexcept{
                return static_cast<reference_type>(*m_value_ptr);
            }
        public:
            void unhandled_exception() noexcept {
                m_exception = std::current_exception();
            }

            void try_rethrow(){
                if(m_exception)[[unlikely]] 
                    std::rethrow_exception(m_exception);
            }
        private:
            value_type * m_value_ptr;
            std::exception_ptr m_exception;
        };

        struct sential{};

        class iterator:private non_copyable{
        public:
            iterator() noexcept = default;

            explicit iterator(std::coroutine_handle<promise> handle) noexcept
            :m_handle(handle)
            {}

            bool operator == (sential s){
                return !m_handle || m_handle.done();
            }
            void operator ++(int) {(void)++(*this);}

            iterator& operator++(){
                m_handle.resume();
                if(m_handle.done())
                m_handle.promise().try_rethrow();
                return *this;
            }

            reference_type & operator*() const noexcept{
                return m_handle.promise().value();
            }

            value_type * operator->() const noexcept{
                return std::addressof(operator*());
            }

        private:
            std::coroutine_handle<promise> m_handle;
        };

    public:
        explicit generator(std::coroutine_handle<promise_type> handle) noexcept
        :m_handle(handle)
        {}

        generator(generator && other) noexcept
        :m_handle(other.m_handle){
            other.m_handle = nullptr;
        }

        ~generator(){
            if(m_handle) m_handle.destory();
        }

        iterator begin() &{
            if(m_handle){
                m_handle.resume();
                if(m_handle.done())
                m_handle.promise().try_rethrow();
            }
            return iterator{m_handle};
        }

        sential end() const noexcept {return {};}

    private:
        std::coroutine_handle<promise> m_handle;
    };

}

#endif