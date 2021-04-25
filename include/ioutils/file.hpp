#ifndef COIO_FILE_HPP
#define COIO_FILE_HPP

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>

#include "file_descriptor_base.hpp"
#include "buffer.hpp"

namespace coio{

    //non copyable 
    class file : file_descriptor_base{
    private:
        explicit file(int fd) noexcept 
        : file_descriptor_base(fd) {}

    public:

        //open a file
        //(path , flag , mode_t) -> awaitable <file>
        static auto openat(const char * path, int flag , mode_t mode = mode_t{})
        -> awaiter_of<file> auto {
            auto ctx = io_context::current_context();
            return ctx->submit_io_task(
                [=](io_uring_sqe* sqe){
                    ::io_uring_prep_openat(sqe , AT_FDCWD , path, flag , mode );
                },
                [](int res , int flag [[maybe_unused]]){
                    if(res < 0) throw make_system_error(-res);
                    else return file{res};
                }
            );
        }

        
        //read
        //(buffer ... ) -> awaitable <size_t>
        //return : count of read bytes
        template<concepts::writeable_buffer T>
        auto read(T &&  buff , off_t off = off_t{}) -> awaiter_of<std::size_t> auto{
            auto ctx = io_context::current_context();
            return ctx->submit_io_task(
                [ ptr = buff.data() , len = buff.size(), off , this]
                (io_uring_sqe * sqe){
                    ::io_uring_prep_read(sqe , this->fd , ptr , len , off);
                },
                [](int res , int flag [[maybe_unused]])-> std::size_t{
                    if(res < 0) throw make_system_error(-res);
                    else return res;
                }
            );
        }

        //readv
        //off_t -> (buffer ... ) -> awaitable <size_t>
        //return : count of read bytes
        template<concepts::writeable_buffer ...T>
        auto readv(off_t off , T&& ... buff) -> awaiter_of<std::size_t> auto{
            auto ctx = io_context::current_context();
            return ctx->submit_io_task(
                [iovecs = make_iovecs(buff...) ,this,off](io_uring_sqe * sqe){
                    ::io_uring_prep_readv(sqe ,this->fd , iovecs.data() , iovecs.size() , off );
                },
                [](int res , int flag [[maybe_unused]])->std::size_t{
                    if(res < 0) throw make_system_error(-res);
                    else return res;
                }
            );
        }

        //write
        //buffer -> off_t -> awaitable<size_t>
        //return : count of write bytes
        template<concepts::buffer T>
        auto write(T && buff , off_t off = off_t{}) -> awaiter_of<std::size_t> auto{
            return io_context::current_context()->submit_io_task(
                [ptr = buff.data() , len = buff.size() , off, this]
                (io_uring_sqe * sqe){
                    ::io_uring_prep_write(sqe , this->fd , ptr , len  ,off);
                },
                [](int res , int flag [[maybe_unused]])->std::size_t{
                    if(res < 0) throw make_system_error(-res);
                    else return res;
                }
            );
        }

        //writev
        //off_t -> (buffer...) -> awaitable<size_t>
        template<concepts::buffer ...T>
        auto writev(off_t off , T && ...buff) -> awaiter_of<std::size_t> auto{
            return io_context::current_context()->submit_io_task(
                [iovecs = make_iovecs(buff...) , this , off]
                (io_uring_sqe * sqe){
                    ::io_uring_prep_writev(sqe , this->fd ,iovecs.data() , iovecs.size() , off);
                },
                [](int res , int flag [[maybe_unused]])->std::size_t{
                    if(res < 0) throw make_system_error(-res);
                    else return res;
                }
            );
        }

    private:


    };

}

#endif