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

namespace coio{

namespace details{

    static inline in_addr translate_addr(const char * str){
        in_addr ret{};
        return ::inet_pton(AF_INET, str, &ret) == 1 ? ret : in_addr{};
    }

    static constexpr inline msghdr default_maker(iovec * iov_ptr , std::size_t iov_len){
        return {};
    }
}

struct address{

    sockaddr_in raw_addr;

    constexpr explicit address() noexcept = default;

    explicit address(uint16_t port , const char * ip = nullptr) noexcept
    :raw_addr {            
        .sin_family = AF_INET , 
        .sin_port = port , 
        .sin_addr = !ip ? in_addr{::htonl(INADDR_ANY)} : details::translate_addr(ip)
    }{}

    bool operator == (const address & other) const noexcept {
        return raw_addr.sin_family == other.raw_addr.sin_family 
            && raw_addr.sin_port == other.raw_addr.sin_port
            && raw_addr.sin_addr.s_addr == other.raw_addr.sin_addr.s_addr ;
    }

    sockaddr * native_ptr() { return reinterpret_cast<sockaddr *> (&raw_addr); }

    constexpr socklen_t socket_len() const { return sizeof(sockaddr_in);}

    constexpr auto proto_family() const { return raw_addr.sin_family; }

    constexpr uint16_t port() const { return raw_addr.sin_port;}

    constexpr uint32_t ip() const { return raw_addr.sin_addr.s_addr;}

};

//socket base
class socket_base : protected file_descriptor_base{

public:
    explicit socket_base() noexcept = default;
    explicit socket_base(int fd ) noexcept : file_descriptor_base(fd){}
    explicit socket_base(int fd , const address & addr) noexcept 
    : file_descriptor_base(fd) , m_addr(addr){}

    explicit socket_base(socket_base && other) = default;
    socket_base & operator= (socket_base && other) = default;

public:
    
    void bind(address addr){
        if(::bind(fd , addr.native_ptr() , addr.socket_len())!= 0) 
            throw make_system_error(errno);
        m_addr = addr;
    }

    const address& local_address () {
        if(m_addr.proto_family() == AF_UNSPEC)[[unlikely]]{
            socklen_t len{};
            if(int ret = ::getsockname(this->fd , m_addr.native_ptr() ,&len);ret !=0 ) [[unlikely]]
                throw make_system_error(errno);
        }
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
    address m_addr;
};

class acceptor;

class tcp_sock : public socket_base{
public:
    explicit tcp_sock() {
        int fd = ::socket(AF_INET , SOCK_STREAM , 0);
        if(fd < 0) throw make_system_error(errno);
        file_descriptor_base::fd = fd;
    }

    tcp_sock(tcp_sock && ) noexcept = default;
    tcp_sock & operator=(tcp_sock &&) noexcept = default;

protected:

    friend acceptor;
    explicit tcp_sock(int fd ,  address addr = address{}) noexcept 
    : socket_base(fd , addr){}

public:

    address get_peer_address() const {
        address addr{};
        socklen_t len = addr.socket_len();
        if(::getpeername(this->fd , addr.native_ptr() , &len) != 0 )
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

class udp_sock : public socket_base{
public:
    explicit udp_sock() {
        int fd = ::socket(AF_INET , SOCK_DGRAM , 0);
        if(fd < 0) throw make_system_error(errno);
        file_descriptor_base::fd = fd;
    }
public:

    template<concepts::writeable_buffer T>
    auto recvfrom(address & addr , T && buff) -> awaiter_of<std::size_t> auto {
        return recvmsg_impl(get_udp_msghdr_maker(addr) , std::forward<T>(buff));
    }

    template<concepts::buffer T>
    auto sendto(address & addr , T && buff) -> awaiter_of<std::size_t> auto {
        return sendmsg_impl(get_udp_msghdr_maker(addr) , std::forward<T>(buff));
    }

private:
    static constexpr auto get_udp_msghdr_maker(address & addr) {
        return [&]{
            return msghdr{
                .msg_name = addr.native_ptr() ,
                .msg_namelen = addr.socket_len() ,
            };
        };
    }
};

//acceptor
class acceptor : public socket_base {
public:
    explicit acceptor(){
        int fd = ::socket(AF_INET , SOCK_STREAM , 0);
        if(fd < 0) throw make_system_error(errno);
        file_descriptor_base::fd = fd;
    }

    acceptor (acceptor && ) = default;

public:

    //listen
    void listen(int max_queue = SOMAXCONN){
        if(::listen(fd , max_queue) != 0 ) 
            throw make_system_error(errno);
    }

    //accept
    //address -> int -> awaitable[socket]
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
    auto connect(address addr) -> awaiter_of<void> auto {
        return io_context::current_context()->submit_io_task(
            [= , this](io_uring_sqe * sqe)mutable{
                ::io_uring_prep_connect(sqe , this->fd, addr.native_ptr() , addr.socket_len());
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




}

#endif