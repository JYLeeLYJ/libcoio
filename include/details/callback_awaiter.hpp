#ifndef COIO_CALLBACK_AWAITER_HPP
#define COIO_CALLBACK_AWAITER_HPP

#include "awaitable.hpp"

namespace coio{

namespace details{

template<class Fn>
struct final_callback_awaiter : std::suspend_always{
    [[no_unique_address]]
    Fn callback;
    void await_suspend(std::coroutine_handle<>) noexcept{
        callback();
    }
};

template<class T , class Fn>
struct with_callback{
    T result ;
    [[no_unique_address]]
    Fn fn ;
};

template<class T , class Fn>
auto forward_with_callback(T && value , Fn && fn ){
    return with_callback<T& , Fn&>{value , fn};
}

}

}

#endif
