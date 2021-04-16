#ifndef COIO_FILE_HPP
#define COIO_FILE_HPP

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdexcept>
#include <filesystem>

#include "common/non_copyable.hpp"
#include "io_context.hpp"

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
            return *this;
        }

    private:
        int fd{-1};
    };

    class file : fd_base{
    private:
        explicit file(int fd) noexcept 
        : fd_base(fd) {}

    public:
        //openat 
        static auto openat(std::filesystem::path p , int flag , mode_t mode = mode_t{}){
            auto ctx = io_context::current_context();
            return ctx->submit_io_task(
                [path = std::move(p) , flag , mode](io_uring_sqe* sqe){
                    ::io_uring_prep_openat(sqe , AT_FDCWD , path.c_str(), flag , mode );
                },
                [](int res , int flag [[maybe_unused]]){
                    if(res < 0) throw std::system_error{-res , std::system_category()};
                    else return file{res};
                }
            );
        }

        //stat
        
        //read

        //write

    };

}

#endif