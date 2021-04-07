#ifndef COIO_GENERATOR_HPP
#define COIO_GENERATOR_HPP

#include <coroutine>
#include <type_traits>
#include <exception>
#include "common/non_copyable.hpp"

namespace coio{

    template<class T>
    class generator:private non_copyable{
    public:
        class promise;
        class iterator;

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

            constexpr auto yield_value(value_type & value) noexcept{
                m_value_ptr = std::addressof(value);
                return std::suspend_always{};
            }

            constexpr auto yield_value(value_type && value) noexcept {
                m_value_ptr = std::addressof(value);
                return std::suspend_always{};
            }

            generator<T> get_return_object() noexcept{
                return generator<T>{ std::coroutine_handle<promise>::from_promise(*this)};
            }
            
            void return_void(){}

            //disable co_await
            template<class U>
            std::suspend_never await_transform(U && value) = delete;

        private:
            friend class iterator;
            value_type * get_if() const noexcept{
                return m_value_ptr;
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

        class iterator{
        public:
            using value_type        = typename generator::value_type;
            using reference         = typename generator::reference_type;
            using pointer           = value_type *;
            using difference_type   = std::ptrdiff_t;
            using iterator_tag      = std::input_iterator_tag;
        public:
            iterator() noexcept = default;
            explicit iterator(std::coroutine_handle<promise> handle) noexcept : m_handle(handle){}
            iterator(const iterator &) = default;
            iterator & operator = (const iterator &) = default;

            bool operator == (sential s) const noexcept{
                return !m_handle || m_handle.done();
            }
            iterator operator ++(int) {return ++(*this);}

            iterator& operator++(){
                m_handle.resume();
                m_handle.promise().try_rethrow();
                return *this;
            }

            //unsafe unwrap
            reference operator*() const noexcept{
                return static_cast<reference_type>(*operator->());
            }

            //unsafe unwrap
            value_type * operator->() const noexcept{
                if(!m_handle) [[unlikely]] std::terminate();
                return m_handle.promise().get_if();
            }

        private:
            std::coroutine_handle<promise> m_handle;
        };

    private:
        explicit generator(std::coroutine_handle<promise_type> handle) noexcept
        :m_handle(handle)
        {}

    public:

        generator(generator && other) noexcept
        :m_handle(other.m_handle){
            other.m_handle = nullptr;
        }

        ~generator(){
            if(m_handle) m_handle.destroy();
        }

        iterator begin() &{
            if(!m_handle) [[unlikely]] std::terminate();
            if(!m_init_begin){
                m_handle.resume();
                m_init_begin = true;
                m_handle.promise().try_rethrow();
            }
            return iterator{m_handle};
        }

        sential end() const noexcept {return {};}

    private:
        bool m_init_begin{false};
        std::coroutine_handle<promise> m_handle;
    };

}

#endif