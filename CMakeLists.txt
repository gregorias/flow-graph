cmake_minimum_required(VERSION 2.8)
project(flow_graph)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pthread")

add_subdirectory(src)
add_subdirectory(example)
