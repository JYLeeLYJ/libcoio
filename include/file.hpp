#ifndef COIO_FILE_HPP
#define COIO_FILE_HPP

#include <liburing.h>
#include <unistd.h>
#include <filesystem>

#include "common/non_copyable.hpp"

namespace coio{

    class fd_base : non_copyable{
    public:
        explicit fd_base() = default;
        explicit fd_base(int fd_) noexcept
        :fd(fd_){}

        ~fd_base() { 
            if(fd >= 0)[[likely]] ::close(fd);
        }

        fd_base(fd_base && other) noexcept 
        : fd(other.fd) {
            other.fd = -1;
        }

        fd_base & operator= (fd_base && other) noexcept {
            if(other.fd != fd)[[likely]]this->~fd_base() ;
            fd = other.fd;
            other.fd = -1;
        }
        
    private:
        int fd{-1};
    };

    class file_base{
    public:
        //openat 
        static void openat(std::filesystem::path p , int flag ){
            // ::io_uring_prep_openat()
        }
        //stat
        
        //read

        //write

    };

}

#endif