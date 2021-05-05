#include <atomic>
#include <thread>
#include <iostream>
#include "ioutils/tcp.hpp"
#include "future.hpp"

using coio::future , coio::io_context ;
using coio::tcp_sock , coio::ipv4 , coio::acceptor ;
using coio::to_bytes , coio::to_const_bytes;

future<void> start_session(tcp_sock<> sock , std::atomic<uint32_t> & bytes_read) {
    // std::cout << "accept connection : " << sock.get_peer_address().to_string() << std::endl; 
    uint32_t read_cnt {};
    try{
    auto buff = std::vector<std::byte>(1024);
    while(true){
        auto n = co_await sock.recv(buff);
        if(n == 0) break;
        auto m = co_await sock.send(std::span{buff.begin() , n});
        assert(m == n);
        read_cnt += n;
    }
    }catch(const std::exception & e){
        std::cout << "exception : " <<  e.what() << std::endl ;
    }
    bytes_read += read_cnt;
}

future<uint32_t> server(io_context & ctx , std::atomic<uint32_t> & bytes_read){
    try{
        auto accpt = acceptor{};
        accpt.bind(ipv4::address{8888});
        accpt.listen();
        while(true){
            auto sock = co_await accpt.accept();
            ctx.co_spawn(start_session(std::move(sock) , bytes_read));
        }
    }catch(const std::exception & e){
        std::cout << "exception : " <<  e.what() << std::endl ;
    }
}

int main(int argc , char * argv[]){
    if(argc!= 2) {
        puts("[error] \n expect parameter : server <thread_cnt> ");
        return 1;
    }

    auto thread_cnt = std::atoi(argv[1]);
    assert(thread_cnt > 0);
    
    std::atomic<uint32_t> cnt{};
    auto tds = std::vector<std::thread>(thread_cnt);
    for(auto & t : tds ){
        t = std::thread([&](){
            io_context ctx{};
            auto _ = ctx.bind_this_thread();
            ctx.co_spawn(server(ctx, cnt));
            ctx.run();   
        });
    }
    for(auto & t : tds) t.join();
}