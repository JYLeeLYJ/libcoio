#include <gtest/gtest.h>

#include "future.hpp"
#include "fmap.hpp"
#include "sync_wait.hpp"
#include "just.hpp"

using coio::future , coio::just , coio::sync_wait , coio::fmap;

TEST(test_fmap , test_just){
    auto f = just(114514) | fmap([](int n){ return -n ;});
    EXPECT_EQ(sync_wait(f) , -114514);

    auto reciver = fmap([](){return 114514;});

    static_assert(std::is_void_v<typename coio::awaitable_traits<just<void>>::await_resume_t>);
    static_assert(std::is_rvalue_reference_v<decltype(sync_wait(just<void>{} | reciver ))>);

    EXPECT_EQ(sync_wait(just<void>{} | reciver) , 114514);

}

TEST(test_fmap , test_with_future){
    auto sender = 
        []()-> future<int>{ co_return 114514 ;}()
        | fmap([](int i) {return 2 * i;}) ;

    EXPECT_EQ(sync_wait(sender) , 114514 * 2);
}