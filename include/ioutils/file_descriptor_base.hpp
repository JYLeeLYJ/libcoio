#ifndef COIO_FILE_BASE_HPP
#define COIO_FILE_BASE_HPP

#include <liburing.h>
#include <unistd.h>
#include "common/non_copyable.hpp"
#include "io_context.hpp"
#include "system_error.hpp"
#include "awaitable.hpp"

namespace coio{

    using concepts::awaiter_of;

    class file_descriptor_base : non_copyable{
    public:
        explicit file_descriptor_base() = default;
        explicit file_descriptor_base(int fd_) noexcept
        :fd(fd_){}

        ~file_descriptor_base() { 
            if(fd >= 0)[[likely]] ::close(fd);
        }

        file_descriptor_base(file_descriptor_base && other) noexcept 
        : fd(other.fd) {
            other.fd = -1;
        }

        file_descriptor_base & operator= (file_descriptor_base && other) noexcept {
            if(other.fd != fd)[[likely]]this->~file_descriptor_base() ;
            fd = other.fd;
            other.fd = -1;
            return *this;
        }
    public:
        //async close
        //will invaild fd after successfully closed
        auto close() -> awaiter_of<void> auto{
            return io_context::current_context()->submit_io_task(
                [this](io_uring_sqe * sqe) {
                    ::io_uring_prep_close(sqe , this->fd);
                },
                [&](int res , int flag){
                    if(res <0 ) throw make_system_error(-res) ;
                    this->fd = -1;
                }
            );
        }

        int native_handle() {
            return fd;
        }

    protected:
        int fd{-1};
    };
}

#endif