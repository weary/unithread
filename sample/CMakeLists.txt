cmake_minimum_required (VERSION 2.8)
project (unithread_sample)

add_subdirectory(.. asiobuilddir) # hmm..

find_package(Boost REQUIRED COMPONENTS system)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(..) # for unithread

add_definitions(-Wall -ggdb3 -std=gnu++0x)

add_executable(asiosample asiosample.cpp)
target_link_libraries(asiosample unithread)
target_link_libraries(asiosample ${Boost_SYSTEM_LIBRARY})

