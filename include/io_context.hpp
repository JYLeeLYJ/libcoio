#ifndef COIO_IOCONTEXT_HPP
#define COIO_IOCONTEXT_HPP

#include <queue>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <ranges>
#include <map>
#include <stop_token>

#include <liburing.h>
#include "awaitable.hpp"
#include "common/non_copyable.hpp"
#include "common/scope_guard.hpp"
#include "details/awaitable_wrapper.hpp"

namespace coio{

class io_context;

namespace concepts{
    template<class F>
    concept task = std::is_invocable_r_v<void ,F>;  // need std::is_nothrow_invocable_r_v<void , F> ?
}

//execution context
class io_context : non_copyable{
private:

    using task_t = std::function<void()>;
    using task_list = std::deque<task_t> ;
    using spawn_task = details::awaitable_wrapper<void>;

    inline static thread_local io_context * this_thread_context {nullptr};

public:

    static io_context * current_context(){
        return this_thread_context;
    }

public:
    explicit io_context(unsigned uring_size = 8 * 1024 ){
        if(auto ret = ::io_uring_queue_init(uring_size , &m_ring, 0 ) ; ret < 0)
            throw std::system_error{-ret , std::system_category()};
    }
    ~io_context(){
        ::io_uring_queue_exit(&m_ring);
    }

    //try bind io_context with this thread
    //RAII : auto release binding.
    [[nodiscard]]
    auto bind_this_thread(){
        m_thid = std::this_thread::get_id();
        this_thread_context = this;
        return scope_guard{.f = []()noexcept{
            this_thread_context = nullptr;
        }};
    }

    bool is_in_local_thread() noexcept{
        return current_context() == this;
    }

    void run(){
        m_is_stopped = false;
        assert_bind();
        while(!m_is_stopped) run_once();
    }

    //can be stopped by io_context itself or stop_token
    void run(std::stop_token token){
        m_is_stopped = false;
        assert_bind();
        while(!m_is_stopped && !token.stop_requested()){
            run_once();
        }
    }

    void request_stop() noexcept{
        m_is_stopped = true;
    }

    //put task into queue .
    //it will be executed later.
    template<concepts::task F>
    void post(F && f){
        if(!is_in_local_thread()){
            std::lock_guard{m_mutex};
            m_remote_tasks.emplace_back(std::forward<F>(f));
        }else 
            m_local_tasks.emplace_back(std::forward<F>(f));
    }

    //put task into queue if in remote thread
    //or run immediately
    template<concepts::task F>
    void dispatch(F && f){
        if(!is_in_local_thread()){
            std::lock_guard{m_mutex};
            m_remote_tasks.emplace_back(std::forward<F>(f));
        }else 
            std::forward<F>(f)();
    }

    //when co_await : 
    //1. run immediately if is in local thread
    //2. else post resume task to remote and suspend
    auto schedule() noexcept{
        struct awaiter : std::suspend_always{
            io_context * context;
            bool await_suspend(std::coroutine_handle<> continuation) {
                if(context->is_in_local_thread()) return false;   
                else context->post([&]()noexcept{
                    continuation.resume();
                });
                return true;    //suspend
            }
        };
        return awaiter{ .context = this};
    }

    //put an awaitable object into context to wait for finished
    template<concepts::awaitable A>
    void co_spawn(A && a){
        dispatch([this , awaitable = std::move(a)]{
            auto co_spwan_entry = co_spwan_entry_point(std::move(awaitable));
            this->m_detach_task[co_spwan_entry.handle] = std::move(co_spwan_entry);
        });
    }

    //TODO:execute an coroutine on current context.

public:
    
    struct async_result {
        int err{0};
        constexpr void set_error(int error ){ err = error;}
    };

    template<class F>
    struct [[nodiscard]] io_awaiter: std::suspend_always , async_result {
        F io_operation;
        void await_suspend(std::coroutine_handle<> handle){
            auto ctx = io_context::current_context();
            ctx->m_pending_coroutines.insert_or_assign(
                handle ,  
                std::ref(static_cast<async_result&>(*this))
            );
            auto *sqe = ::io_uring_get_sqe(&ctx->m_ring);
            //TODO:process full sqe uring
            assert(sqe);
            ::io_uring_sqe_set_data(sqe , handle.address());
            // fill io info into sqe
            (void)io_operation(sqe);
        }
        int await_resume() noexcept{return err;}
    };

    template<std::invocable<io_uring_sqe*> F>
    static auto emit_io_task(F && f){
        return io_awaiter<F>{ .io_operation = std::move(f)};
    }

private:

    struct entry_final_awaiter : std::suspend_always{
        void await_suspend(std::coroutine_handle<> handle){
            auto ctx = io_context::current_context();
            //erase later;
            ctx->dispatch([=](){
                if(auto it = ctx->m_detach_task.find(handle);it != ctx->m_detach_task.end())
                    ctx->m_detach_task.erase(it);                
            });
        }
    };

    template<concepts::awaitable A>
    spawn_task co_spwan_entry_point( A && a){
        (void)co_await std::forward<A>(a);
        co_await entry_final_awaiter{};
    }

private:

    void assert_bind(){
        if(!current_context()) 
            throw std::logic_error{"need invoke bind_this_thread() when before run()."};
        //another io_context runs on this thread
        else if(current_context() != this )  
            throw std::logic_error{"more than one io_context run in this thread."};
        //this io_context runs on the other thread
        else if( m_thid != std::this_thread::get_id()) 
            throw std::logic_error{"io_context cannot invoke run() on multi-threads."};
    }

    void run_once(){
        thread_local unsigned empty_cnt{0};
        std::size_t cnt{};

        //process IO complete
        cnt+= ::io_uring_cq_ready(&m_ring);
        for_each_cqe([this](io_uring_cqe * cqe) noexcept{
            auto handle = std::coroutine_handle<>::from_address(::io_uring_cqe_get_data(cqe));
            assert(handle);
            if(auto it = m_pending_coroutines.find(handle);
            it != m_pending_coroutines.end())[[likely]]{
                it->second.get().err = cqe->res;
                m_pending_coroutines.erase(it);
                handle.resume();
            }
            //TODO: define runtime error ? callback not found .
        });

        cnt+= resolved_remote_task();
        cnt+= resolved_local_task();

        //IO submit 
        if(::io_uring_sq_ready(&m_ring)>0)
            ::io_uring_submit(&m_ring);
        
        //avoid busy loop
        if(cnt == 0 && ++empty_cnt == 256) [[unlikely]]
            empty_cnt = 0 , std::this_thread::yield();
    }

    std::size_t resolved_remote_task(){
        if(!m_remote_tasks.empty()){
            task_list tasks{};
            {
                std::lock_guard guard{m_mutex};
                tasks.swap(m_remote_tasks);
            }
            for(auto & task : tasks) task();
            return tasks.size();
        }
        else 
            return 0;
    }

    std::size_t resolved_local_task(){
        if(!m_local_tasks.empty()){
            task_list tasks{};
            tasks.swap(m_local_tasks);
            for(auto & task : tasks) task();
            return tasks.size();
        }
        else 
            return 0;
    }

    template<class F>
    requires std::is_nothrow_invocable_v<F , io_uring_cqe *>
    void for_each_cqe(F && f) noexcept{
        io_uring_cqe * cqe{};
        unsigned head;
        io_uring_for_each_cqe(&m_ring , head , cqe){
            assert(cqe);
            (void)std::forward<F>(f)(cqe);
            ::io_uring_cqe_seen(&m_ring , cqe);
        }
    }

private:

    io_uring m_ring{}; 

    std::mutex m_mutex;
    task_list m_remote_tasks;

    task_list m_local_tasks;
    std::atomic<bool> m_is_stopped{false};
    std::thread::id m_thid;

    //waiting for io complete
    std::map<std::coroutine_handle<>, std::reference_wrapper<async_result>> m_pending_coroutines;
    //co_spwan ownership 
    std::map<std::coroutine_handle<> , spawn_task> m_detach_task;
};

}

#endif