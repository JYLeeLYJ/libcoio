#ifndef COIO_TCP_HPP
#define COIO_TCP_HPP

#include "socket_base.hpp"

namespace coio{

namespace details{
    static constexpr inline 
    msghdr default_maker(iovec * iov_ptr , std::size_t iov_len){  return {};}
}

class acceptor;

class tcp_sock : public socket_base<ipv4::tcp>{
protected:
    using address_t = ipv4::tcp::address_type;
public:

    tcp_sock() = default ;
    tcp_sock(tcp_sock && ) noexcept = default;
    tcp_sock & operator=(tcp_sock &&) noexcept = default;

protected:

    friend acceptor;
    explicit tcp_sock(int fd ,  address_t addr = address_t{}) noexcept 
    : socket_base(fd , addr){}

public:

    address_t get_peer_address() const {
        address_t addr{};
        socklen_t len = addr.len();
        if(::getpeername(this->fd , addr.ptr() , &len) != 0 )
            throw make_system_error(errno);
        return addr;
    }

public:

    template<concepts::writeable_buffer T>
    auto recv(T && buff) -> awaiter_of<std::size_t> auto {
        return io_context::current_context()->submit_io_task(
            [ptr = buff.data() , len = buff.size(),this](io_uring_sqe * sqe){
                ::io_uring_prep_recv(sqe , this->fd , ptr , len , 0);
            },
            [](int res , int flag) -> std::size_t{
                return res <0 ? throw make_system_error(-res) : res;
            }
        );
    }

    template<concepts::buffer T>
    auto send(T && buff) -> awaiter_of<std::size_t> auto {
        return io_context::current_context()->submit_io_task(
            [ptr = buff.data() , len = buff.size(),this](io_uring_sqe * sqe){
                ::io_uring_prep_send(sqe , this->fd , ptr , len , MSG_NOSIGNAL);
            },
            [](int res , int flag) -> std::size_t {
                return res <0 ? throw make_system_error(-res) : res;
            }
        );
    }

    template<concepts::writeable_buffer ...T >
    auto recvmsg(T && ... buff) -> awaiter_of<std::size_t> auto{
        return recvmsg_impl(details::default_maker , std::forward<T>(buff)...);
    }

    template<concepts::buffer ... T>
    auto sendmsg(T && ... buff) -> awaiter_of<std::size_t> auto{
        return sendmsg_impl(details::default_maker , std::forward<T>(buff)...);
    }

    enum close_how : int{
        shutdown_read = 0,
        shutdown_write = 1,
        shutdown_all = 2,
    };

    // aysnc shutdown 
    auto shutdown(close_how how) -> awaiter_of<void> auto{
        return io_context::current_context()->submit_io_task(
            [=,this](io_uring_sqe * sqe){
                ::io_uring_prep_shutdown(sqe , this->fd , how);
            },
            [](int res , int flag) {
                if(res < 0) throw make_system_error(-res);
            }
        );
    }
};



//acceptor
class acceptor : public socket_base<ipv4::tcp> {
public:
    
    acceptor() = default ;

public:

    //listen
    void listen(int max_queue = SOMAXCONN){
        if(::listen(fd , max_queue) != 0 ) 
            throw make_system_error(errno);
    }

    //accept
    //address_t -> int -> awaitable[socket]
    //TODO : flag enum
    auto accept(int flag = 0) -> awaiter_of<tcp_sock> auto {
        return io_context::current_context()->submit_io_task(
            [&](io_uring_sqe *sqe){
                ::io_uring_prep_accept( sqe , this->fd , nullptr , nullptr ,flag);
            },
            [](int res , int flag [[maybe_unused]])-> tcp_sock{
                if(res < 0) throw make_system_error(-res);
                return tcp_sock{res};
            }
        );
    }

};

//connector
class connector : public tcp_sock{
public:

    explicit connector() noexcept = default;

    //connect
    //awaitable[void]
    auto connect(address_t addr) -> awaiter_of<void> auto {
        return io_context::current_context()->submit_io_task(
            [= , this](io_uring_sqe * sqe)mutable{
                ::io_uring_prep_connect(sqe , this->fd, addr.ptr() , addr.len());
            },
            [&](int res , int flat[[maybe_unused]]){
                if(res < 0) throw make_system_error(-res);
            }
        );
    }

    tcp_sock& socket() {
        return *this;
    }
};


} //namespace coio


#endif