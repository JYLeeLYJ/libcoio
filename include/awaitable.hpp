#ifndef COIO_AWAITABLE_HPP
#define COIO_AWAITABLE_HPP

#include <coroutine>
#include <concepts>
#include <type_traits>

namespace coio{

    namespace concepts{

        namespace details {

            template<class T>
            struct is_coroutine_handle 
            : std::false_type{};

            template<class Promise>
            struct is_coroutine_handle<std::coroutine_handle<Promise>> 
            : std::true_type{};

            template<class T>
            concept suspend_result = 
                std::same_as<bool ,T > || 
                std::same_as<void , T> || 
                is_coroutine_handle<T>::value; 

            template<class T>
            concept await_transformable = requires (T t){
                typename T::promise_type;
                std::declval<T::promise_type>().await_transform();
            };

            template<await_transformable T>
            struct transform_result{
                using type = std::remove_reference_t<decltype(
                    std::declval<T::promise_type>().await_transform()
                )>;
            };

        }

        template<class T>
        concept awaiter = requires(T t){
            {t.await_ready()} -> std::convertible_to<bool>;
            {t.await_suspend(std::coroutine_handle<>{})} -> details::suspend_result;
            t.await_resume();
        };

        template<class T>
        concept awaitable = 
            awaiter<T> || 
            details::await_transformable<T> ||
            requires(T t){
                {operator co_await(t)} -> awaiter ;
            }||
            requires(T t){
                {t.operator co_await()} -> awaiter;
            };
    }

    template<concepts::awaiter T>
    auto get_awaiter(T && t) -> std::remove_reference_t<T>;

    template<concepts::awaitable T>
    requires requires(T t){t.operator co_await();}
    auto get_awaiter(T && t) -> decltype(static_cast<T&&>(t).operator co_await());

    template<concepts::awaitable T>
    requires concepts::details::await_transformable<T>
    auto get_awaiter(T && t) -> decltype(get_awaiter(std::declval<T>().await_transform()));

    template<concepts::awaitable T>
    auto get_awaiter(T && t) -> decltype(operator co_await(static_cast<T&&>(t)));


    template<concepts::awaitable T>
    struct awaitable_traits{
        using awaiter_t         = decltype(get_awaiter(std::declval<T>()));
        using await_result_t    = decltype(std::declval<awaiter_t>().await_resume());
    };
}

#endif 