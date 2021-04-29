#include <atomic>
#include <thread>
#include <jthread>
#include "tcp.hpp"
#include "future.hpp"

using coio::future , coio::io_context ;
using coio::tcp_sock , coio::ipv4 , coio::acceptor ;
using coio::to_bytes , coio::to_const_bytes;

future<uint32_t> server(io_context & ctx , std::atomic<uint32_t> & bytes_read){
    auto accpt = acceptor{};
    accpt.bind(ipv4::address{8888});
    accpt.listen();
    while(true){
        auto sock = co_await accpt.accept();
        ctx.co_spawn([& , sock = std::move(sock)]() mutable ->future<void>{
            uint32_t read_cnt {};
            try{
            auto buff = std::vector<std::byte>(1024);
            while(true){
                auto n = co_await sock.recv(buff);
                if(n == 0) break;
                auto m = co_await sock.send(buff);
                assert(m == n);
                read_cnt += n;
            }
            }catch(...){}
            bytes_read += read_cnt;
        }());
    }
}

int main(int argc , char * argv[]){
    if(argc!= 2) 
        puts("error .\n expect use : server <thread_cnt> ");

    auto thread_cnt = std::atoi(argv[1]);
    assert(thread_cnt > 0);
    
    std::atomic<uint32_t> cnt{};
    auto thds = std::vector<std::jthread>(thread_cnt);
    for(auto & t : tds ){
        t = std::jthread([](std::stop_token token){
            io_context ctx{};
            auto _ = ctx.bind_this_thread();
            ctx.co_spawn(server(ctx, cnt));
            ctx.run(token);   
        });
    }
    for(auto & t : tds) t.join();
}