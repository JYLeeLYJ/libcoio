#include <gtest/gtest.h>

#include "flat_map.hpp"
#include "fmap.hpp"
#include "future.hpp"
#include "just.hpp"
#include "sync_wait.hpp"
#include <functional>

using coio::future, coio::just, coio::sync_wait, coio::fmap, coio::flat_map;
using coio::concepts::awaitable, coio::concepts::awaiter,
    coio::concepts::coroutine;

TEST(test_fmap, test_just) {
  awaiter auto j = just(114514);
  auto f = j | fmap([](int n) { return -n; });
  EXPECT_EQ(sync_wait(f), -114514);

  auto receiver = fmap([]() { return 114514; });

  static_assert(std::is_void_v<
                typename coio::awaitable_traits<just<void>>::await_resume_t>);
  static_assert(
      std::is_rvalue_reference_v<decltype(sync_wait(just<void>{} | receiver))>);

  EXPECT_EQ(sync_wait(just<void>{} | receiver | fmap(std::negate<int>{})),
            -114514);
}

TEST(test_fmap, test_with_future) {
  awaitable auto sender = []() -> future<int> { co_return 114514; }() |
                                      fmap([](int i) { return 2 * i; });

  EXPECT_EQ(sync_wait(sender), 114514 * 2);
}

TEST(test_fmap, test_flatmap) {
  awaitable auto s1 = []() -> future<int> { co_return 114514; }();
  coroutine<int> auto and_then = [](int i) -> future<int> { co_return i * 2; };
  coio::flat_map_transform<decltype(and_then) &> transf = flat_map(and_then);
  awaitable auto s2 = s1 | transf;
  EXPECT_EQ(sync_wait(s2), 114514 * 2);
}

TEST(test_fmap, test_lifetime) {

  struct awaiter : std::suspend_never {
    bool *p{};
    awaiter(bool *ptr) : p(ptr) {}
    awaiter(awaiter &&a) : p(a.p) { a.p = nullptr; }
    awaiter &operator=(awaiter &&a) {
      std::swap(p, a.p);
      a.p = nullptr;
      return *this;
    }
    ~awaiter() {
      if (p)
        *p = true;
    }
  };

  bool is_destory{false};
  { // rvalue will be move into a
    auto a = awaiter{&is_destory} | fmap([]() { return 114514; });
    EXPECT_FALSE(is_destory);
    sync_wait(a);
  }
  // destory when a destory
  EXPECT_TRUE(is_destory);

  { // rvalue will be move into a
    is_destory = false;
    auto a =
        awaiter{&is_destory} | flat_map([] { return std::suspend_never{}; });
    EXPECT_FALSE(is_destory);
  }
  EXPECT_TRUE(is_destory); // awaiter destory when a destory

  is_destory = false;
  auto a = flat_map(awaiter{&is_destory}, [] { return std::suspend_never{}; });
  EXPECT_FALSE(is_destory);
  sync_wait(a);
  // destory when " co_await awaiter{&is_destory} " finished
  EXPECT_TRUE(is_destory);

  is_destory = false;
  sync_wait(fmap(awaiter{&is_destory}, [] { return 114514; }));
  EXPECT_TRUE(is_destory);

  is_destory = false;
  auto b = awaiter{&is_destory};
  auto f = flat_map(b, [] { return std::suspend_never{}; });
  EXPECT_TRUE(b.p);
  sync_wait(f);
  EXPECT_TRUE(b.p);
  EXPECT_FALSE(is_destory);
}

//----------------------------------------------------------
// GCC BUG TRACK : https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99575
// https://godbolt.org/z/anWWjb4j4

// struct future_t{
//     struct promise_type{
//         future_t get_return_object() noexcept{
//             return
//             {std::coroutine_handle<promise_type>::from_promise(*this)};
//         }
//         auto initial_suspend() noexcept{
//             return std::suspend_never();
//         }
//         auto final_suspend() noexcept{
//             return std::suspend_never();
//         }
//         void return_void(){}
//         void unhandled_exception(){std::terminate();}
//     };
//     std::coroutine_handle<> handle;
// };

// template<class T>
// future_t foo(T t){
//     co_await std::forward<T>(t);    //will invoke move constructor here , no
//     matter T = awaiter& or awaiter
// }

// template<class T>
// auto make_foo(T && t){
//     return foo<T>(std::forward<T>(t));
// }

// TEST(test_coroutine_bug , test_co_await_forward){

//     struct awaiter : std::suspend_never{
//         bool * p{};
//         awaiter(bool * ptr):p(ptr){}
//         awaiter(awaiter && a):p(a.p){a.p = nullptr;}
//         awaiter& operator=(awaiter && a){std::swap(p , a.p); a.p =
//         nullptr;return *this;} ~awaiter(){ if(p) *p = true; }
//     };
//     bool is_des =false;
//     auto a = awaiter{&is_des};
//     auto b = make_foo(a);

//     EXPECT_FALSE(is_des);
//     EXPECT_TRUE(a.p);
//     b.handle.resume();
//     EXPECT_TRUE(a.p);
//     EXPECT_FALSE(is_des);
// }