#include <asio.hpp>
#include <ranges>
#include <array>
#include <thread>

using namespace asio::ip;

asio::awaitable<void> echo_client(){
    auto executor = co_await asio::this_coro::executor;
    auto socket = tcp::socket{executor};
    try{
        co_await socket.async_connect({tcp::v4() , 14514} ,asio::use_awaitable );
        std::array<char , 32> buffer{};
        for(auto i : std::views::iota(1, 10)){
            snprintf(buffer.data() , buffer.size() , "hello %d" , i);
            auto send_buf = asio::buffer(buffer.data() , strlen(buffer.data()));
            auto n = co_await socket.async_send( send_buf, asio::use_awaitable);
            n = co_await socket.async_receive(send_buf , asio::use_awaitable);
            buffer[n] = '\0';
            printf("send back %s\n" , buffer.data());
        }
    }catch(const std::exception & e){
        // printf("client exception : %s\n",e.what());
    }
}

asio::awaitable<void> do_echo(tcp::socket sock){
    std::array<char , 32> buffer{};
    try{
        while(sock.is_open()){
            auto buff = asio::buffer(buffer.data() , 32);
            auto n = co_await sock.async_receive(buff , asio::use_awaitable);
            buffer[n] = '\0';
            printf("receive %s , " , buffer.data());
            co_await sock.async_send(asio::buffer(buffer.data() , n) , asio::use_awaitable);
        }
    }
    catch(const std::exception & e){
        // printf("server exception...\n");
    }
}

asio::awaitable<void> echo_server(asio::io_context & ioc){
    auto executor = co_await asio::this_coro::executor;
    auto acceptor = tcp::acceptor{executor , {tcp::v4() ,14514}};
    while(true){
        auto socket = tcp::socket{executor};
        co_await acceptor.async_accept(socket , asio::use_awaitable);
        asio::co_spawn( ioc , do_echo(std::move(socket)) ,asio::detached);
    }
}

int main(){
    auto t1 = std::thread([](){
        asio::io_context ctx{1};
        asio::co_spawn(ctx , echo_client , asio::detached);
        ctx.run();
        printf("client fin.\n");
    });
    auto t2 = std::thread([&](){
        asio::io_context ctx{1};
        asio::co_spawn(ctx , echo_server(ctx) , asio::detached);
        ctx.run();
        printf("server fin!\n");
    });
    t2.join();
    t1.join();
}