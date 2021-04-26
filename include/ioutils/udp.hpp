#ifndef COIO_UDP_HPP
#define COIO_UDP_HPP

#include "socket_base.hpp"

namespace coio{

class udp_sock : public socket_base<ipv4::udp>{
    using address_t = ipv4::udp::address_type ;
public:
    udp_sock () = default;
public:

    template<concepts::writeable_buffer T>
    auto recvfrom(address_t & addr , T && buff) -> awaiter_of<std::size_t> auto {
        return recvmsg_impl(get_udp_msghdr_maker(addr) , std::forward<T>(buff));
    }

    template<concepts::buffer T>
    auto sendto(address_t & addr , T && buff) -> awaiter_of<std::size_t> auto {
        return sendmsg_impl(get_udp_msghdr_maker(addr) , std::forward<T>(buff));
    }

private:
    static constexpr auto get_udp_msghdr_maker(address_t & addr) {
        return [&]{
            return msghdr{
                .msg_name = addr.ptr() ,
                .msg_namelen = addr.len() ,
            };
        };
    }
};

}

#endif