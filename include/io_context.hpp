#ifndef COIO_IOCONTEXT_HPP
#define COIO_IOCONTEXT_HPP

#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <map>
#include <deque>
#include <stop_token>
#include <chrono>
#include <cassert>

#include <liburing.h>
#include "awaitable.hpp"
#include "common/non_copyable.hpp"
#include "common/scope_guard.hpp"
#include "details/awaitable_wrapper.hpp"

namespace coio{

class io_context;

namespace concepts{
    template<class F>
    concept task = 
        std::is_invocable_r_v<void ,F> && // need std::is_nothrow_invocable_r_v<void , F> ?
        std::copy_constructible<F>;  //std::function requires
}


inline constexpr auto invalid_cpuno = std::numeric_limits<uint32_t>::max();

struct ctx_opt{
    uint32_t ring_size {4096};                  //ring should be power of 2 . 
    uint32_t sq_cpu_affinity{invalid_cpuno};    //set cpu affinity for kernel sq thread 
    uint32_t sq_poll_idle{};                    //set sq thread empty polling timeout  
    bool sq_polling{false};                     //enable kernel sq thread polling
    bool io_polling{false};                     //enable io polling instead of hardwa
};

//execution context
class io_context : non_copyable{
private:

    using task_t = std::function<void()>;
    using task_list = std::deque<task_t> ;
    using spawn_task = details::awaitable_wrapper<void>;

    inline static thread_local io_context * this_thread_context {nullptr};

public:

    static io_context * current_context() noexcept{
        assert(this_thread_context);
        return this_thread_context;
    }

public:
    explicit io_context( ctx_opt option = {}){
        auto params = make_params(option);
        auto ret = ::io_uring_queue_init_params(option.ring_size , &m_ring , &params);
        if(ret < 0)
            throw std::system_error{-ret , std::system_category()};
    }
    ~io_context(){
        ::io_uring_queue_exit(&m_ring);
    }

    //try bind io_context with this thread
    //RAII : auto release binding.
    [[nodiscard]]
    auto bind_this_thread() noexcept{
        m_thid = std::this_thread::get_id();
        this_thread_context = this;
        return scope_guard{
            []()noexcept{ this_thread_context = nullptr;}
        };
    }

    bool is_in_local_thread() noexcept{
        return this_thread_context == this;
    }

    bool is_polling_mode() const noexcept {
        return m_ring.flags & (IORING_SETUP_SQPOLL | IORING_SETUP_IOPOLL);
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

    //test if this feature is supported by io_uring 
    bool test_feature(unsigned fea) const noexcept{
        return m_ring.features & fea ;
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
            m_remote_tasks.emplace_back(std::move(f));
            // m_remote_tasks.emplace_back(std::forward<F>(f));
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
                else context->post([=]()noexcept{
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
        auto co_spawn_entry = co_spwan_entry_point<A>(std::forward<A>(a));
        if(is_in_local_thread()){
            insert_entry_and_start(std::move(co_spawn_entry));
        }else{
            std::lock_guard guard{m_mutex};
            m_remote_co_spwan.push_back(std::move(co_spawn_entry));
        }
    }

    //TODO:execute an coroutine on current context.


    //
    std::size_t current_coroutine_cnt() const noexcept{
        return m_detach_task.size();
    }

public:
    
    struct async_result {
        int res;
        int flag;
        std::coroutine_handle<> continuation;
        constexpr void set_result(int res , int flag) noexcept{ 
            this->res = res; this->flag = flag;
        }
        constexpr void set_continuation(std::coroutine_handle<> handle) noexcept{
            continuation = handle;
        }
        void resume() noexcept{
            assert(continuation.address());
            if(continuation)[[likely]] continuation.resume();
        }
    };

    template<class F , class R>
    struct [[nodiscard]] io_awaiter: std::suspend_always , async_result {
        [[no_unique_address]] F io_operation;
        [[no_unique_address]] R get_result;
        
        void await_suspend(std::coroutine_handle<> handle) 
        noexcept(noexcept(io_operation(nullptr))){
            auto ctx = io_context::current_context();
            auto *sqe = ::io_uring_get_sqe(&ctx->m_ring);
            //TODO:process full sqe uring
            assert(sqe);
            // fill io info into sqe
            (void)io_operation(sqe);
            set_continuation(handle);
            ::io_uring_sqe_set_data(sqe , static_cast<async_result*>(this));
        }
        auto await_resume() noexcept(std::is_nothrow_invocable_v<R , int , int>){
            return get_result(res,flag);
        }
    };

    template<std::invocable<io_uring_sqe*> FSubmit , std::invocable<int,int> FResult>
    static auto submit_io_task(FSubmit && fsubmit , FResult && fget){
        return io_awaiter<FSubmit , FResult>{
            .io_operation = std::forward<FSubmit>(fsubmit) , 
            .get_result = std::forward<FResult>(fget)
        };
    }

private:

    struct entry_final_awaiter : std::suspend_always{
        io_context * ctx;
        void await_suspend(std::coroutine_handle<> handle){
            //erase later;
            ctx->dispatch([= , this](){
                if(auto it = ctx->m_detach_task.find(handle);it != ctx->m_detach_task.end())
                    ctx->m_detach_task.erase(it);                
            });
        }
    };

    template<concepts::awaitable A>
    spawn_task co_spwan_entry_point( A a){
        //GCC BUG TRACK : https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99575
        // https://godbolt.org/z/anWWjb4j4
        // (void)co_await std::forward<A>(a);   // A& will also be moved here (on gcc)
        try{
        (void)co_await reinterpret_cast<A&&>(a);
        }catch(...){}
        co_await entry_final_awaiter{.ctx = this};
    }

private:

    io_uring_params make_params(const ctx_opt & option){
        io_uring_params params{};
        if(option.io_polling) params.flags |= IORING_SETUP_IOPOLL;
        if(option.sq_polling) {
            params.flags |= IORING_SETUP_SQPOLL;
            if(option.sq_cpu_affinity != invalid_cpuno
            && option.sq_cpu_affinity < std::thread::hardware_concurrency()) 
                params.flags |= IORING_SETUP_SQ_AFF;
            params.sq_thread_cpu = option.sq_cpu_affinity;
            params.sq_thread_idle = option.sq_poll_idle;
        }
        return params;
    }

    void assert_bind(){
        if(!this_thread_context) 
            throw std::logic_error{"need invoke bind_this_thread() when before run()."};
        //another io_context runs on this thread
        else if(this_thread_context != this )  
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
            auto result = reinterpret_cast<async_result*>(::io_uring_cqe_get_data(cqe));
            assert(result);
            result->set_result(cqe->res , cqe->flags);
            result->resume();
            //TODO: define runtime error ? callback not found .
        });

        cnt+= resolve_remote_task();
        cnt+= resolve_local_task();

        //IO submit 
        ::io_uring_submit(&m_ring);
        
        // TODO : differ between POLL mode and normal
        // avoid busy loop
        if(cnt != 0) 
            empty_cnt = 0 ;
        else if(++empty_cnt == 64) [[unlikely]]{
            using namespace std::chrono_literals;
            empty_cnt = 0 ;
            // std::this_thread::sleep_for(1ms);
            std::this_thread::yield();
        }
            
    }

    void insert_entry_and_start(spawn_task && entry){
        auto handle = entry.handle();
        m_detach_task.insert_or_assign(handle , std::move(entry));
        handle.resume();
    }

    void resolve_remote_coroutine(){
        if(m_remote_co_spwan.empty()) return ;
        std::vector<spawn_task> tasks{};
        {
        std::lock_guard guard{m_mutex};
        std::swap(tasks , m_remote_co_spwan);
        }

        for(auto & entry : tasks){
            insert_entry_and_start(std::move(entry));
        }
    }

    std::size_t resolve_remote_task(){
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

    std::size_t resolve_local_task(){
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
    std::vector<spawn_task> m_remote_co_spwan;

    task_list m_local_tasks;
    std::atomic<bool> m_is_stopped{false};
    std::thread::id m_thid;
    
    //co_spwan ownership 
    std::map<std::coroutine_handle<> , spawn_task> m_detach_task;//
};

}

#endif