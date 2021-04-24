#ifndef COIO_SYSTEM_ERROR_HPP
#define COIO_SYSTEM_ERROR_HPP

#include <system_error>

namespace coio{

    static inline std::system_error make_system_error(int err){
        return std::system_error{err , std::system_category()};
    }

}

#endif