cmake_minimum_required (VERSION 2.8)
project (unithread)

add_definitions(-Wall -ggdb3 -std=gnu++0x)

#add_subdirectory(../shared shared)


#include_directories(..)

add_library(unithread unithread.cpp)
#target_link_libraries(unithread misc)

add_executable(unithread_test unithread_main.cpp)
target_link_libraries(unithread_test unithread)
