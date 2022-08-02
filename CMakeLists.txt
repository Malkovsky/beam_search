cmake_minimum_required(VERSION 3.22)
project(pybind_template)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(third_party/pybind11)
add_subdirectory(third_party/Catch2)
include_directories(include)
#pybind11_add_module(pybind_template main.cpp)

add_executable(beam_search_tests
        tests/beam_search_tree_tests.cpp
        tests/run_tests.cpp)
target_link_libraries(beam_search_tests PRIVATE Catch2::Catch2)
