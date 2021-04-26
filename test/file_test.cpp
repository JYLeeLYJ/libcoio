#include <gtest/gtest.h>
#include <filesystem>
#include <span>
#include "ioutils/file.hpp"
#include "future.hpp"
#include "io_context.hpp"

TEST(test_file , test_trivally_read_write){
    namespace fs = std::filesystem;
    using namespace std::literals;
    coio::io_context ctx{};
    auto _ = ctx.bind_this_thread();
    std::exception_ptr exp{};
    ctx.co_spawn([&]()->coio::future<void>{
        try{
            if(!fs::exists("./tmp")) fs::create_directory("./tmp");
            if(fs::exists("./tmp/test.txt")) fs::remove("./tmp/test.txt");
            //open
            auto file = co_await coio::file::openat("./tmp/test.txt" , O_CREAT|O_RDWR );
            std::string write_str{"hello world."};
            //write
            int n = co_await file.write(coio::to_bytes(write_str));
            EXPECT_EQ(n , write_str.size());
            //read
            std::array<char,16> read_buff{};
            n = co_await file.read(coio::to_bytes(read_buff));
            EXPECT_EQ(n , write_str.size());
            EXPECT_EQ(std::string_view(read_buff.data() , n) , "hello world."sv);
            //readv
            std::array<char , 6> b1{};
            std::array<char , 6> b2{};
            n = co_await file.readv(
                0,
                coio::to_bytes(b1),//std::as_writable_bytes(std::span{b1}) , 
                coio::to_bytes(b2) //std::as_writable_bytes(std::span{b2})
            );
            
            EXPECT_EQ(n , write_str.size());
            EXPECT_EQ(std::string_view(b1.data() , 6) , "hello ");
            EXPECT_EQ(std::string_view(b2.data() , 6) , "world.");

        }catch(...){
            exp = std::current_exception();
        }
        ctx.request_stop();
        co_return;
    }());
    ctx.run();
    if(exp)std::rethrow_exception(exp);
}