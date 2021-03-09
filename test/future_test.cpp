#include <gtest/gtest.h>
#include "future.hpp"

using coio::future;

TEST(test_future , test_start){
    bool start = false;
    auto func1 = [&]()->future<void>{
        start =true;
        co_return;
    };

    auto assert_false = [&]{ASSERT_FALSE(start);};
    auto assert_true = [&]{ASSERT_TRUE(start);};

    //start until await 
    // func1.poll_wait();
    [&]()->future<void>{
        auto t = func1();
        assert_false();
        co_await t;
        assert_true();
    }().poll_wait();

}
