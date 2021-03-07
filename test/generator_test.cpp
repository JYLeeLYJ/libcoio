#include <gtest/gtest.h>
#include <concepts>
#include <string>
#include "generator.hpp"

using coio::generator;
using namespace std::literals;

//reference cppcoro tests cases : 
//https://github.com/lewissbaker/cppcoro/blob/master/test/generator_tests.cpp

TEST(test_generator , test_value_type){
    auto g = []() -> generator<float> {
        co_yield 1.0f;
        co_yield 2.0f;
    }();

    auto it = g.begin();
    ASSERT_EQ(*it , 1.0f);
    ++it;
    ASSERT_EQ(*it , 2.0f);
    ++it;
    ASSERT_EQ(it , g.end());
}

TEST(test_generator , test_reference_type){
    auto g = []() -> generator<float&> {co_yield 1.0f;}();
    auto it = g.begin();

    auto g2 = []() -> generator<float &&>{co_yield 2.0f;}();
    auto it2 = g2.begin();

    auto g3 = []() -> generator<std::string>{
        co_yield "fffff"s;
    }();
    auto it3 = g3.begin();    
    
    ASSERT_EQ(*it , 1.0f);
    ASSERT_EQ(*it2 , 2.0f);
    ASSERT_EQ(*it3 , "fffff"s);
}

TEST(test_generator , test_const_type){
	auto fib = []() -> generator<const std::uint64_t>{
		std::uint64_t a = 0, b = 1;
		while (true){
			co_yield b;
			b += std::exchange(a, b);
		}
	};

	std::uint64_t count = 0;
	for (auto i : fib()){
		if (i > 1'000'000) {
			break;
		}
		++count;
	}
    ASSERT_EQ(count , 30);
}

namespace {
    template<std::invocable<> F>
    struct on_exit_t{
        F f;
        on_exit_t(F _f) : f(std::forward<F>(f)){};
        ~on_exit_t() noexcept(noexcept(f())){ f();}
    };
}

TEST(test_generator , test_lifetime){
    bool destructed = false;
	bool completed = false;
    
	auto f = [&]() -> generator<int>{
        struct OnExit{
            bool & ref_val;
            OnExit(bool & value) : ref_val(value){};
            ~OnExit(){ref_val = true;};
        };
        auto onexit = OnExit{destructed};
        // got GCC10.2 internal bugs
        // auto onexit = on_exit_t{[&]{destructed = true;}};
		co_yield 1;
		co_yield 2;
		completed = true;
	};

	{
		auto g = f();
		auto it = g.begin();
		auto itEnd = g.end();
        ASSERT_NE(it , itEnd);
		ASSERT_EQ(*it , 1u);
		ASSERT_FALSE(destructed);
	}

    ASSERT_FALSE(completed);
	ASSERT_TRUE(destructed);
}

TEST(test_generator , test_generator_execute_flow){
    bool reachedA = false;
	bool reachedB = false;
	bool reachedC = false;
	auto f = [&]() -> generator<int>{
		reachedA = true;
		co_yield 1;
		reachedB = true;
		co_yield 2;
		reachedC = true;
	};

	auto gen = f();
	ASSERT_FALSE(reachedA);
	auto iter = gen.begin();
	ASSERT_TRUE(reachedA);
	ASSERT_FALSE(reachedB);
	ASSERT_EQ(*iter ,1);
	++iter;
	ASSERT_TRUE(reachedB);
	ASSERT_FALSE(reachedC);
	ASSERT_EQ(*iter , 2);
	++iter;
	ASSERT_TRUE(reachedC);
	ASSERT_EQ(iter , gen.end());
}

TEST(test_generator , test_generator_throws){
    struct X{};

    auto g = []() -> generator<int>{
        throw X{};
        co_return;
    }();

    ASSERT_ANY_THROW(g.begin());
    
    auto g2 = []() -> generator<int>{
        co_yield 1;
        throw X{};
    }();

    auto it = g2.begin();
    ASSERT_ANY_THROW(++it);
    ASSERT_EQ(it , g2.end());
}

//TODO: test fmap 