#include <gtest/gtest.h>
#include <iostream>

#include "common/ref.hpp"
#include "future.hpp"
#include "when_all.hpp"
#include "sync_wait.hpp"

using coio::future ,  coio::ref;
using coio::sync_wait , coio::when_all;

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

TEST(test_future, test_result_lifetime){
    static uint cnt{0};
    struct count {
        count() { ++ cnt;}
        ~count() {--cnt;}
        count(const count& t){++cnt;}
        count& operator=(const count&t){++cnt;return *this;}
    };

    //temp var count{} will be move into future
    auto f = []()->future<count>{
        co_return count{};  //copy
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

    {
        [[maybe_unused]]
        auto && result = sync_wait(f2(count{}));
        //warning : cause dangling reference , 
        //sync_wait returns rvalue reference , not prvalue
        //same_as co_await
        EXPECT_EQ(cnt , 0); 
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
    auto f = [&]()-> future<ref<int>>{
        co_return ref{value};
    };

    sync_wait(
    [&]()->future<void>{
        {
            decltype(auto) result = co_await f();
            static_assert(std::is_same_v<decltype(result),ref<int>&&>);
            EXPECT_EQ(&result.get() , &value);
        }

        {
            auto t = f();
            decltype(auto) result = co_await t;
            static_assert(std::is_same_v<decltype(result),ref<int>&>);
            EXPECT_EQ(&result.get() , &value);
        }
    }());
}

TEST(test_future , test_when_all){
    int value = 4;

    {
        auto make_future = [&]()->future<void> {
            --value;
            co_return;
        };

        std::vector<future<void>> v{};
        v.emplace_back(make_future());
        v.emplace_back(make_future());
        sync_wait(when_all(std::move(v)));
        EXPECT_EQ(value , 2);
    }


    {
        auto make_future = [&]()->future<int> {
            --value;
            co_return value;
        };

        std::vector<future<int>> v{};
        v.emplace_back(make_future());
        v.emplace_back(make_future());

        std::vector<int> results = sync_wait(when_all(std::move(v)));
        EXPECT_EQ(results.size() , 2);
        for(auto i = 2 ; int v: results)
            EXPECT_EQ(--i , v);
    }

    EXPECT_EQ(value , 0);

    //-------------------------------------

    struct awaiter : std::suspend_never{
        bool * p{};
        awaiter(bool * ptr):p(ptr){}
        awaiter(awaiter && a):p(a.p){a.p = nullptr;}
        awaiter& operator=(awaiter && a){std::swap(p , a.p); a.p = nullptr;return *this;}
        ~awaiter(){ if(p) *p = true; }
    };

    //when_all must owns awaiter when it is rvalue.
    bool is_destory {false};
    {
        auto when = when_all(awaiter{&is_destory});
        EXPECT_FALSE(is_destory);
        sync_wait(when);
    }
    EXPECT_TRUE(is_destory);

    //not moving 
    is_destory = false;
    auto waiter = awaiter{&is_destory};
    auto w_a = when_all(waiter);
    EXPECT_FALSE(is_destory);
    sync_wait(w_a);
    EXPECT_FALSE(is_destory);
}

TEST(test_future , test_exec_async){
    bool before = false;
	bool after = false;
    coio::awaitable_counter counter{.cnt = 1};
	auto f = [&]() -> future<void>{
		before = true;
		co_await counter;
		after = true;
	};

	sync_wait([&]() -> future<void>{
		auto t = f();

        EXPECT_FALSE(before);

		co_await when_all(
			[&]() -> future<void>{
				co_await t;
				EXPECT_TRUE(before);
				EXPECT_TRUE(after);
			}(),
			[&]() -> future<void>
			{
				EXPECT_TRUE(before);
				EXPECT_FALSE(after);
				counter.notify_complete_one();
                EXPECT_TRUE(after);
                co_return;
			}());
	}());
}

//move count and zero copy
TEST(test_future , test_move_and_copy_counting){
    static uint moven = 0 , copyn = 0;
    struct foo{
        foo() = default;
        foo(foo&& ){++moven;}
        foo(const foo&) {++copyn;}
        foo& operator=(foo&&){++moven; return *this;} 
        foo& operator=(const foo&&){++copyn; return *this;}
    };
    
    [[maybe_unused]]
    //2.move into a
    auto a = sync_wait([]()->future<foo>{
        co_return foo{};//1. move here into future
    }());
    
    EXPECT_EQ(copyn , 0);
    EXPECT_EQ(moven , 2);

    moven = 0;
    auto future1 = []()->future<foo>{ co_return foo{};}();
    [[maybe_unused]]
    auto & result1 = sync_wait(future1); //return lvalue ref cause 'future1' is lvalue.
    EXPECT_EQ(copyn , 0);
    EXPECT_EQ(moven , 1);
}

//TODO : test when_all / sync_wait when exception occur 
