include_directories("../src/")
add_executable(example example.cpp)
target_link_libraries(example flow_graph)
