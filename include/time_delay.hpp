#ifndef COIO_TIME_DELAY_HPP
#define COIO_TIME_DELAY_HPP

#include <chrono>
#include <coroutine>
#include "common/match_template.hpp"
#include "system_error.hpp"
#include "io_context.hpp"

namespace coio{

namespace concepts{

    template<class D>
    concept duration = is_match_template_v<std::chrono::duration , D> ;

}

namespace details{
    
    using namespace std::chrono;

    template<class R , class P>
    constexpr auto get_seconds(const duration<R,P> & d){
        return floor<seconds>(d);
    }

    template<class R , class P>
    constexpr auto get_nanosecs (const duration<R,P> & d ){
        return duration_cast<nanoseconds>( d < 1s ? d : d - get_seconds(d));
    }

    template<class R , class P>
    constexpr auto to_kernel_timespec(const duration<R,P> & d) {
        return __kernel_timespec{
            .tv_sec = get_seconds(d).count() , 
            .tv_nsec = get_nanosecs(d).count()
        };
    }

}

template<concepts::duration D>
auto time_delay(D && d) -> concepts::awaiter_of<void> auto {
    return io_context::current_context()->submit_io_task(
        [spec = details::to_kernel_timespec(d)]
        (io_uring_sqe * sqe) mutable {
            ::io_uring_prep_timeout(sqe , &spec , 0  ,0);
        },
        [](int res , int flag ) {
            if(-res != ETIME && res < 0) [[unlikely]]
                throw make_system_error(-res);
        }
    );
}

}

#endif