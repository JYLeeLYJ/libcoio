#ifndef COIO_FILE_HPP
#define COIO_FILE_HPP

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>

#include "common/non_copyable.hpp"
#include "io_context.hpp"
#include "buffer.hpp"

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

    protected:
        int fd{-1};
    };

    //non copyable 
    class file : fd_base{
    private:
        explicit file(int fd) noexcept 
        : fd_base(fd) {}

    public:

        //open a file
        //(path , flag , mode_t) -> awaitable <file>
        static auto openat(const char * path, int flag , mode_t mode = mode_t{}){
            auto ctx = io_context::current_context();
            return ctx->submit_io_task(
                [=](io_uring_sqe* sqe){
                    ::io_uring_prep_openat(sqe , AT_FDCWD , path, flag , mode );
                },
                [](int res , int flag [[maybe_unused]]){
                    if(res < 0) throw std::system_error{-res , std::system_category()};
                    else return file{res};
                }
            );
        }

        
        //read
        //(buffer ... ) -> awaitable <int>
        //return : count of read bytes
        template<concepts::writeable_buffer T>
        auto read(T &&  buff , off_t off = off_t{}){
            auto ctx = io_context::current_context();
            return ctx->submit_io_task(
                [ ptr = buff.data() , len = buff.size(), off , this]
                (io_uring_sqe * sqe){
                    ::io_uring_prep_read(sqe , this->fd , ptr , len , off);
                },
                [](int res , int flag [[maybe_unused]])-> std::size_t{
                    if(res < 0) throw std::system_error{-res , std::system_category()};
                    else return res;
                }
            );
        }

        //readv
        //(buffer ... ) -> awaitable <int>
        //return : count of read bytes
        template<concepts::writeable_buffer ...T>
        auto readv(T&& ... buff){
            auto ctx = io_context::current_context();
            return ctx->submit_io_task(
                [iovecs = make_iovecs(buff...) ,this](io_uring_sqe * sqe){
                    ::io_uring_prep_readv(sqe ,this->fd , iovecs.data() , iovecs.size() , 0 );
                },
                [](int res , int flag [[maybe_unused]])->std::size_t{
                    if(res < 0) throw std::system_error{-res , std::system_category()};
                    else return res;
                }
            );
        }

        //write
        template<concepts::buffer T>
        auto write(T && buff , off_t off = off_t{}){
            return io_context::current_context()->submit_io_task(
                [ptr = buff.data() , len = buff.size() , off, this](io_uring_sqe * sqe){
                    ::io_uring_prep_write(sqe , this->fd , ptr , len  ,off);
                },
                [](int res , int flag [[maybe_unused]])->std::size_t{
                    if(res < 0) throw std::system_error{-res , std::system_category()};
                    else return res;
                }
            );
        }

        //writev
        template<concepts::buffer ...T>
        auto write(T && ...buff){
            return io_context::current_context()->submit_io_task(
                [iovecs = make_iovecs(buff...) , this](io_uring_sqe * sqe){
                    ::io_uring_prep_writev(sqe , this->fd ,iovecs.data() , iovecs.size() , 0);
                },
                [](int res , int flag [[maybe_unused]])->std::size_t{
                    if(res < 0) throw std::system_error{-res , std::system_category()};
                    else return res;
                }
            );
        }

    private:


    };

}

#endif