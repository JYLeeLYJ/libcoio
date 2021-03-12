#include <gtest/gtest.h>
#include <iostream>

#include "future.hpp"
#include "sync_wait.hpp"

using coio::future , coio::sync_wait;

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
    EXPECT_EQ(sync_wait(f) , 114514);
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
        auto &result = sync_wait(ft);
        EXPECT_EQ(cnt , 1);
    }
    EXPECT_EQ(cnt , 0);

    auto f2 = [](count c) -> future<count>{
        co_return c;
    };

    {
        future<count> ft = f2(count{});
        EXPECT_EQ(cnt , 1);
    }
    EXPECT_EQ(cnt , 0);
}

TEST(test_future , test_return_convertiable){
    struct foo{
        explicit foo(int i) {} 
    };

    struct foo2{
        operator int(){
            return 1;
        }
    };
    [[maybe_unused]]
    auto f = []()->future<foo>{
        co_return 1;
    };
    auto f2= []()->future<int>{
        co_return foo2{};
    }; 

    EXPECT_EQ(sync_wait(f2()) , 1);
}

TEST(test_future , test_return_reference){
    int value{3};
    auto f = [&]()-> future<int&>{
        co_return value;
    };

    sync_wait(
    [&]()->future<void>{
        {
            decltype(auto) result = co_await f();
            // static_assert(std::is_same_v<decltype(result),int&>);
            EXPECT_EQ(&result , &value);
        }

        {
            auto t = f();
            decltype(auto) result = co_await t;
            // static_assert(std::is_same_v<decltype(result),int&>);
            EXPECT_EQ(&result , &value);
        }
    }());
}

// TEST(test_future , test_exec_async){
//     bool reachedBeforeEvent = false;
// 	bool reachedAfterEvent = false;
// 	cppcoro::single_consumer_event event;
// 	auto f = [&]() -> cppcoro::task<>
// 	{
// 		reachedBeforeEvent = true;
// 		co_await event;
// 		reachedAfterEvent = true;
// 	};

// 	cppcoro::sync_wait([&]() -> cppcoro::task<>
// 	{
// 		auto t = f();

// 		CHECK(!reachedBeforeEvent);

// 		(void)co_await cppcoro::when_all_ready(
// 			[&]() -> cppcoro::task<>
// 			{
// 				co_await t;
// 				CHECK(reachedBeforeEvent);
// 				CHECK(reachedAfterEvent);
// 			}(),
// 			[&]() -> cppcoro::task<>
// 			{
// 				CHECK(reachedBeforeEvent);
// 				CHECK(!reachedAfterEvent);
// 				event.set();
// 				CHECK(reachedAfterEvent);
// 				co_return;
// 			}());
// 	}());
// }