#include <gtest/gtest.h>
#include "generator.hpp"

using coio::generator;

TEST(test_generator , test_value){
    // generator<int> ints; //nodefault constructor;
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

TEST(test_generator , test_reference){
    auto g = []() -> generator<float&> {co_yield 1.0f;}();
    auto it = g.begin();

    auto g2 = []() -> generator<float &&>{co_yield 2.0f;}();
    auto it2 = g2.begin();
    
    ASSERT_EQ(*it , 1.0f);
    ASSERT_EQ(*it2 , 2.0f);
}