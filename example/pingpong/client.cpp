#include <span>
#include <numeric>
#include <iostream>
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
    uint64_t expect_read = 1024* times;

    try{

    auto conn = connector{};
    co_await conn.connect(ipv4::address{8888});
    auto &sock = conn.socket();
    auto buff = std::vector<std::byte>(1024);

    while(times --){
        [[maybe_unused]]
        auto n = co_await sock.send(buff);
        auto m = co_await sock.recv(buff);
        if(m == 0) break;
        bytes_read+= m;
    }

    }catch(...){
    }

    std::cout << "total read " << bytes_read<< std::endl;
    std::cout << "expect " << expect_read << std::endl;
    co_return bytes_read;
} 

void start(uint conns , uint times , std::atomic<uint64_t> & bytes){
    
    auto ctx = io_context{};
    auto _ = ctx.bind_this_thread();
    
    auto task = 
        [&]()->future<void>{
            //TODO : time set
            std::vector<future<uint64_t>> futures;
            while(conns -- ){
                futures.emplace_back(client(ctx , times));
            }
            auto results = co_await when_all(std::move(futures));
            bytes+= std::accumulate(results.begin() , results.end(), 0 );
            ctx.request_stop();
        };

    ctx.post([&]{
        ctx.co_spawn(task());
    });
    ctx.run();
}


int main(int argc , char * argv[]){

    //read params "client <thread cnt> <pingpong times> <connection/thread>"
    if(argc!= 4) {
        std::cout << "[error] expect use : client <thread_cnt>  <echo times>  <connection per thread>\n" ;
        std::cout << "got : ";
        for(int i = 0 ; i < argc ; ++ i) std::cout << argv[i] ;
        std::cout << std::endl;
        return 1;
    }

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