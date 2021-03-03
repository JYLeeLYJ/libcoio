cmake_minimum_required(VERSION 3.16)
project(coio)

find_package(gtest REQUIRED)
target_include_directories(coio PRIVATE ${PROJECT_SOURCE_DIR}/include)

set(SOURCES 
test/generator_test.cpp
)

add_executable(coio_tests ${SOURCES})
target_compile_features(coio_tests PRIVATE cxx_std_20)
target_compile_options(coio_tests PRIVATE "-fcoroutines")
