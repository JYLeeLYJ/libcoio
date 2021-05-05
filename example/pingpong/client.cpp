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

constexpr uint64_t KB = 1024;
constexpr uint64_t MB = 1024 * 1024;

future<uint64_t> client( io_context & ctx , uint times) {
    uint64_t bytes_read {};
    [[maybe_unused]]
    uint64_t expect_read = KB* times;

    try{

    auto conn = connector{};
    co_await conn.connect(ipv4::address{8888});
    auto &sock = conn.socket();
    auto buff = std::vector<std::byte>(KB);

    while(times --){
        [[maybe_unused]]
        auto n = co_await sock.send(buff);
        auto m = co_await sock.recv(buff);
        if(m == 0) break;
        bytes_read+= m;
    }

    }catch(const std::exception & e){
        std::cout << "exception : " << e.what() << std::endl;
    }

    // std::cout << "total read " << bytes_read<< std::endl;
    // std::cout << "expect " << expect_read << std::endl;
    // assert(bytes_read != expect_read );
    co_return bytes_read;
} 

std::atomic<double>  g_throughput {0.0};
std::atomic<int> thread_id {0};

void start(uint conns , uint times , std::atomic<uint64_t> & bytes){
    
    auto ctx = io_context{};
    auto _ = ctx.bind_this_thread();
    
    auto task = 
        [&]()->future<void>{
            using namespace std::chrono;
            std::vector<future<uint64_t>> futures;
            //for all connections
            while(conns -- ){
                futures.emplace_back(client(ctx , times));
            }
            
            auto beg = steady_clock::now();
            auto results = co_await when_all(std::move(futures));
            auto end = steady_clock::now();
            
            auto this_bytes = std::accumulate(results.begin() , results.end(), 0ull );
            auto cost_tm = duration_cast<milliseconds>(end-beg) ;
            auto this_throughput = (this_bytes / 1024.0) / cost_tm.count() ;
            g_throughput += this_throughput;
            bytes += this_bytes;

            std::cout 
                << "thread" << ++thread_id << " pingpong cost " << cost_tm.count() << " ms , "
                << "throughput " << this_throughput << " MB/S" << std::endl;

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
        for(int i = 0 ; i < argc ; ++ i) std::cout << argv[i] << " ";
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
    std::atomic<uint64_t> bytes{};
    auto cnt = thread_cnt;
    while(cnt --){
        tds.emplace_back(std::thread([&]{
            start(connect_ptd , echo_times , bytes);
        }));
    }

    for(auto & t : tds) t.join();

    std::cout << thread_cnt << " threads client throughput = " << g_throughput.load() << " MB/s , read " << bytes << " bytes."<< std::endl;
}