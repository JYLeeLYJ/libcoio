#include <gtest/gtest.h>

#include "file.hpp"
#include "future.hpp"
#include "io_context.hpp"

TEST(test_file , test_openat){
    coio::io_context ctx{};
    auto _ = ctx.bind_this_thread();
    ctx.co_spawn([&]()->coio::future<void>{
        try{
            [[maybe_unused]]
            auto file = co_await coio::file::openat("./tmp/test.txt" , O_CREAT );
        }catch(...){}

        ctx.request_stop();
        co_return;
    }());
    ctx.run();
}