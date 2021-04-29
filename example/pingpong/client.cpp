#include <span>
#include <numeric>
#include <thread>
#include <chrono>

#include "future.hpp"
#include "io_context.hpp"
#include "ioutils/tcp.hpp"
#include "when_all.hpp"

using coio::future , coio::io_context ;
using coio::tcp_sock , coio::ipv4 , coio::connector ;
using coio::to_bytes , coio::to_const_bytes , coio::when_all;

future<uint64_t> client( io_context & ctx , uint times) {
    uint64_t bytes_read {};

    try{

    auto conn = connector{};
    co_await conn.connect(ipv4::address{8888});
    auto &sock = conn.socket();
    auto buff = std::vector<std::byte>(1024);

    while(times --){
        auto n = co_await sock.send(buff);
        auto m = co_await sock.recv(buff);
        assert(m == n);
        bytes_read+= m;
    }

    }catch(...){
    }

    co_return bytes_read;
} 

void start(uint conns , uint times , std::atomic<uint64_t> & bytes){
    
    auto ctx = io_context{};
    auto _ = ctx.bind_this_thread();

    std::vector<future<uint32_t>> futures;
    while(conns -- ){
        futures.emplace_back(client(ctx , times));
    }
    ctx.post([&]{
        ctx.co_spawn([&]()->future<void>{
            //TODO : time set
            auto results = co_await when_all(futures);
            bytes+= std::accumulate(results.begin() , results.end(), 0 );
            ctx.request_stop();
        }());
    });
    ctx.run();
}


int main(int argc , char * argv[]){

    //read params "client <thread cnt> <pingpong times> <connection/thread>"
    if(argc!= 2) 
        puts("error .\n expect use : client <thread_cnt>  <echo times>  <connection per thread>");

    auto thread_cnt = std::atoi(argv[1]);
    auto echo_times = std::atoi(argv[2]);
    auto connect_ptd= std::atoi(argv[3]);

    assert(connect_ptd > 0);
    assert(thread_cnt > 0);
    assert(echo_times > 0);

    //co_spawn all
    std::vector<std::thread> tds {};
    std::atomic<uint64_t> cnt{};
    while(thread_cnt --){
        tds.emplace_back(std::thread([&]{
            start(connect_ptd , echo_times , cnt);
        }));
    }

    for(auto & t : tds) t.join();

    //TODO : print result.
}