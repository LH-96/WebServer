cmake_minimum_required(VERSION 3.0.0)
project(test)

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/include")

set(PROJECT_TEST_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/test/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_TEST_BINARY_DIR})

set(CMAKE_CXX_STANDARD 14)

add_executable(threadPool_test threadPool_test.cpp)

find_package(Threads)

target_link_libraries(threadPool_test ${CMAKE_THREAD_LIBS_INIT})
