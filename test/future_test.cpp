#include <gtest/gtest.h>
#include "future.hpp"
#include "sync_wait.hpp"

using coio::future;

TEST(test_future , test_start){
    bool start = false;
    auto func1 = [&]()->future<void>{
        start =true;
        co_return;
    };

    //start until await 
    auto f = [&]()->future<int>{
        auto t = func1();
        EXPECT_FALSE(start);
        co_await t;
        EXPECT_TRUE(start);
        co_return 114514;
    }();
    // EXPECT_EQ(f.poll_wait() ,114514);
    EXPECT_EQ(coio::sync_wait(f) , 114514);
}
