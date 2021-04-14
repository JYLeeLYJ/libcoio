#ifndef COIO_IOCHANNEL_HPP
#define COIO_IOCHANNEL_HPP

#include <functional>
#include <coroutine>

#include "io_context.hpp"

namespace coio{

    // template<class T>
    // struct result {
    //     std::align_union_t<int , T> value;
    //     bool is_err;
    //     void set_error(int err);
    //     void set_result(T && t);
    // };

    // [[nodiscard]]
    // struct co_async_awaiter : std::suspend_always{
    //     int err{0};
    //     bool await_suspend(std::coroutine_handle<> handle){
    //         auto * ctx = io_context::current_context();
    //         if(!ctx) [[unlikely]]
    //             throw std::runtime_error{"cannot get io_context in this_thread."};
    //         //set callback
            
    //         //start task
    //     }

    //     int await_resume(){
    //         return err;
    //     }
        
    // };
    

    
}

#endif