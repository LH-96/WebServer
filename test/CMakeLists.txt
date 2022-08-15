cmake_minimum_required(VERSION 3.0.0)
project(test)

set(PROJECT_TEST_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/test/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_TEST_BINARY_DIR})

set(CMAKE_CXX_STANDARD 14)

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/include")

file(GLOB_RECURSE ALL_TESTS *.cpp)
foreach(v ${ALL_TESTS})
    string(REGEX MATCH "test/.*" relative_path ${v})
    string(REGEX REPLACE "test/" "" target_name ${relative_path})
    string(REGEX REPLACE ".cpp" "" target_name ${target_name})

    add_executable(${target_name} ${v})
endforeach()

find_package(Threads)

target_link_libraries(threadPool_test ${CMAKE_THREAD_LIBS_INIT})
