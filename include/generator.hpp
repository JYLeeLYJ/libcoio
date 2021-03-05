#ifndef COIO_GENERATOR_HPP
#define COIO_GENERATOR_HPP

#include <coroutine>
#include <type_traits>
#include <exception>

namespace coio{

    struct non_copyable{
        non_copyable() noexcept = default;
        non_copyable(const non_copyable&) = delete;
        non_copyable & operator=(const non_copyable &) = delete;
    };

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

        class iterator:private non_copyable{
        public:
            explicit iterator() noexcept = default;
            explicit iterator(std::coroutine_handle<promise> handle) noexcept
            :m_handle(handle)
            {}

            explicit iterator(iterator && it) noexcept
            :m_handle(it.m_handle){
                it.m_handle = nullptr;
            }

            iterator & operator= (iterator &&it) noexcept{
                m_handle = it.m_handle;
                it.m_handle = nullptr;
            }

            bool operator == (sential s) const noexcept{
                return !m_handle || m_handle.done();
            }
            void operator ++(int) {(void)++(*this);}

            iterator& operator++(){
                m_handle.resume();
                m_handle.promise().try_rethrow();
                return *this;
            }

            //unsafe unwrap
            reference_type operator*() const noexcept{
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
            m_handle.resume();
            m_handle.promise().try_rethrow();
            return iterator{m_handle};
        }

        sential end() const noexcept {return {};}

    private:
        std::coroutine_handle<promise> m_handle;
    };

}

#endif