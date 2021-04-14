#ifndef COIO_SCOPEGUARD_HPP
#define COIO_SCOPEGUARD_HPP

#include <type_traits>
#include "common/non_copyable.hpp"

namespace coio{

    template<class F>   
    requires std::is_nothrow_invocable_r_v<void ,F> 
    struct scope_guard : non_copyable{
        F f;
        ~scope_guard(){ f(); }
    };

}

#endif