#ifndef COIO_SOCKET_BASE_HPP
#define COIO_SOCKET_BASE_HPP

#include <liburing.h>
#include <arpa/inet.h>
#include <string_view>

#include "file_descriptor_base.hpp"
#include "io_context.hpp"
#include "common/non_copyable.hpp"
#include "system_error.hpp"
#include "buffer.hpp"
#include "protocol.hpp"

namespace coio{

//socket base
template<concepts::protocal Proto >
class socket_base : public file_descriptor_base{
public:
    using address_t = typename Proto::address_type ;

public:
    socket_base() {
        int fd = ::socket(m_proto.domain() , m_proto.type() , 0);
        if(fd < 0) throw make_system_error(errno);
        file_descriptor_base::fd = fd;
    }
    explicit socket_base(int fd ) noexcept : file_descriptor_base(fd){}
    explicit socket_base(int fd , const address_t & addr) noexcept 
    : file_descriptor_base(fd) , m_addr(addr){}

    explicit socket_base(socket_base && other) = default;
    socket_base & operator= (socket_base && other) = default;

public:
    
    void set_reuse_address() {
        int reuse = 1;
        int ret = ::setsockopt(file_descriptor_base::fd , SOL_SOCKET , SO_REUSEADDR , &reuse , sizeof(int));
        if(ret != 0) throw make_system_error(errno);
    }

    void set_reuse_port(){
        int reuse = 1;
        int ret = ::setsockopt(file_descriptor_base::fd , SOL_SOCKET , SO_REUSEPORT , &reuse , sizeof(int));
        if(ret != 0) throw make_system_error(errno);
    }

    void bind(address_t addr ){
        if(::bind(fd , addr.ptr() , addr.len())!= 0) 
            throw make_system_error(errno);
        m_addr = addr;
    }

    const address_t& local_address () {
        return m_addr;
    }

protected:

    //recvmsg
    template<class F , concepts::writeable_buffer ... T>
    requires requires (F f) { { f() } -> std::same_as<msghdr> ;}
    auto recvmsg_impl(F && make_msghdr , T && ... buff) -> awaiter_of<std::size_t> auto{
        return io_context::current_context()->submit_io_task(
            [iovecs = make_iovecs(std::forward<T>(buff)...) 
            ,msg = make_msghdr()
            ,this]
            (io_uring_sqe * sqe) mutable{
                msg.msg_iov = iovecs.data(); msg.msg_iovlen = iovecs.size();
                ::io_uring_prep_recvmsg(sqe , this->fd , &msg , 0);
            },
            [](int res , int flag) -> std::size_t {
                if(res < 0 ) throw make_system_error(-res);
                else return res;
            }
        );
    }

    //sendmsg
    template< class F , concepts::buffer ...T>
    requires requires (F f) { { f() } -> std::same_as<msghdr> ;}
    auto sendmsg_impl(F && make_msghdr , T && ... buff) -> awaiter_of<std::size_t> auto{
        return io_context::current_context()->submit_io_task(
            [iovecs = make_iovecs(std::forward<T>(buff)...) 
            ,msg = make_msghdr()
            ,this]
            (io_uring_sqe * sqe)mutable{
                msg.msg_iov = iovecs.data(); msg.msg_iovlen = iovecs.size();
                ::io_uring_prep_sendmsg(sqe , this->fd , &msg , MSG_NOSIGNAL);
            },
            [](int res , int flag) -> std::size_t {
                if(res < 0 ) throw make_system_error(-res);
                else return res;
            }
        );
    }

protected:
    address_t m_addr;   //this host address
    [[no_unique_address]] 
    Proto   m_proto;
};

}

#endif