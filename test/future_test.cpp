#include <gtest/gtest.h>
#include "future.hpp"

using coio::future;

TEST(test_future , test_start){
    bool start = false;
    auto func1 = [&]()->future<void>{
        start =true;
        co_return;
    }();

    //start until await 
    
}
