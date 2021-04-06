#include <gtest/gtest.h>

#include "future.hpp"
#include "fmap.hpp"
#include "sync_wait.hpp"
#include "just.hpp"
#include "flat_map.hpp"

using coio::future , coio::just , coio::sync_wait , coio::fmap , coio::flat_map;
using coio::concepts::awaitable ,coio::concepts::awaiter , coio::concepts::coroutine ;

TEST(test_fmap , test_just){
    awaiter auto j = just(114514);
    auto f = j | fmap([](int n){ return -n ;});
    EXPECT_EQ(sync_wait(f) , -114514);

    auto receiver = fmap([](){return 114514;});

    static_assert(std::is_void_v<typename coio::awaitable_traits<just<void>>::await_resume_t>);
    static_assert(std::is_rvalue_reference_v<decltype(sync_wait(just<void>{} | receiver ))>);

    EXPECT_EQ(sync_wait(just<void>{} | receiver) , 114514);

}

TEST(test_fmap , test_with_future){
    awaitable auto sender = 
        []()-> future<int>{ co_return 114514 ;}()
        | fmap([](int i) {return 2 * i;}) ;

    EXPECT_EQ(sync_wait(sender) , 114514 * 2);
}

TEST(test_fmap , test_flatmap){
    awaitable auto s1 = []()->future<int>{co_return 114514;}();
    coroutine<int> auto and_then = [](int i)->future<int>{co_return i * 2;};
    coio::flat_map_transform<decltype(and_then)&>transf = flat_map(and_then);
    awaitable auto s2 = s1 | transf;
    EXPECT_EQ(sync_wait(s2) , 114514 * 2);
}