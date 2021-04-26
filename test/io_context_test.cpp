#include <gtest/gtest.h>
#include <future>
#include <unistd.h>
#include <sys/syscall.h>

#include "io_context.hpp"
#include "future.hpp"
#include "time_delay.hpp"

TEST(test_io_context , test_syscall_implement){
    io_uring_params p{};
    io_uring ring{};
    ASSERT_NE( ::io_uring_queue_init_params(1024 , &ring , &p), -38);  // IO_URING not implement
    ::io_uring_queue_exit(&ring);
}

TEST(test_io_context , post_trival_task){
    coio::io_context ctx{};
    uint cnt{0};
    
    auto _ = ctx.bind_this_thread();

    for([[maybe_unused]] auto i : std::ranges::iota_view{0,10})
        ctx.post([&]{
            ++cnt; 
        });
    
    ctx.post([&]{
        ctx.request_stop();
    });
    ctx.run();

    //finish after consume all task
    ASSERT_EQ(cnt ,10);

    ctx.post([&]{
        ctx.request_stop();
    });
    ctx.run();
    ASSERT_EQ(cnt , 10);

    ASSERT_TRUE(ctx.is_in_local_thread());

    ctx.post([&]{
        cnt = 114514;
    });
    auto future = std::async([&]{
        for( [[maybe_unused]] auto i : std::ranges::iota_view{0,10})
            ctx.post([&]{
                --cnt;
            });
    });
    ctx.post([&]{
        ctx.request_stop();
    });
    future.wait();
    ctx.run();

    //local run after remote in run_once() design
    ASSERT_EQ(cnt , 114514);
}

TEST(test_io_context , test_bind_and_dispatch){
    coio::io_context ctx{};
    //post into remote queue because ctx is not bound with this thread.
    bool is_executed = false;
    ctx.dispatch([&]{
        is_executed = true;
        ctx.request_stop();
    });
    EXPECT_FALSE(is_executed);
#ifdef NDEBUG
    EXPECT_EQ(coio::io_context::current_context() , nullptr);
#endif
    //assert not binding and throw logic_error
    ASSERT_ANY_THROW(ctx.run());
    {
        auto ctx_gaurd = ctx.bind_this_thread();
        bool is_executed = false;
        EXPECT_TRUE(ctx.is_in_local_thread());
        EXPECT_EQ(coio::io_context::current_context() , &ctx);
        ctx.dispatch([&]{
            is_executed = true;
        });
        EXPECT_TRUE(is_executed);
    }
#ifdef NDEBUG
    EXPECT_EQ(coio::io_context::current_context() , nullptr);
#endif
}

//test stop token
TEST(test_io_context , test_stop_token){
    std::promise<void> promise{};
    auto future = promise.get_future();
    auto worker = std::jthread([&](std::stop_token token){
        promise.set_value_at_thread_exit();
        try{
            coio::io_context ctx{};
            auto guard = ctx.bind_this_thread();
            ctx.run(token);
        }catch(...){
            promise.set_exception(std::current_exception());
        }
    });
    worker.request_stop();
    future.wait();
    ASSERT_NO_THROW(future.get());
}

//test co_spawn
TEST(test_io_context , test_co_spawn){
    coio::io_context ctx{};
    auto guard = ctx.bind_this_thread();
    
    bool is_execute = false;
    bool is_destory = true;
    ctx.co_spawn([&]()->coio::future<void>{
        //will be destory before this coro destory
        coio::scope_guard gaurd = [&]()noexcept{
            is_destory = true;
        };
        is_execute = true;
        co_return;
    }());
    ctx.post([&]{
        ctx.request_stop();
    });
    EXPECT_TRUE(is_execute);
    EXPECT_TRUE(is_destory);
    EXPECT_TRUE(ctx.current_coroutine_cnt() == 0);
    ctx.run();
}

TEST(test_io_context , test_delay){
    using namespace std::chrono_literals ;

    auto ctx = coio::io_context{};
    auto _ = ctx.bind_this_thread();

    auto spec = coio::details::to_kernel_timespec(1500ms);
    EXPECT_EQ(spec.tv_sec , 1);
    EXPECT_EQ(spec.tv_nsec , 500 * 1000 * 1000);

    auto ptr = std::exception_ptr{};

    ctx.co_spawn([&]()->coio::future<void>{
        using namespace std::chrono;

        try{

        auto beg = steady_clock::now();   
        co_await coio::time_delay(123ms);

        auto end = steady_clock::now();
        auto duration = duration_cast<microseconds>(end - beg);
        EXPECT_GE(duration.count() , 123);

        }catch(...){
            ptr = std::current_exception();
        }
        ctx.request_stop();
    }());

    ctx.run();
    if(ptr) std::rethrow_exception(ptr);
}

//TODO:test sqe full
