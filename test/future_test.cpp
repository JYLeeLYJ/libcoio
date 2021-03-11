#include <gtest/gtest.h>
#include <iostream>

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

TEST(test_future, test_destroy_result){
    static uint cnt{0};
    struct count {
        count() { ++ cnt;}
        ~count() {--cnt;}
        count(const count& t){++cnt;}
        count& operator=(const count&t){++cnt;return *this;}
    };

    auto f = []()->future<count>{
        co_return count{};
    };

    //destory result when future destory
    {
        auto ft = f();
        EXPECT_EQ(cnt , 0);
        [[maybe_unused]]
        auto &result = coio::sync_wait(ft);
        EXPECT_EQ(cnt , 1);
    }
    EXPECT_EQ(cnt , 0);
}

//TODO : return from convertiable type to result type
