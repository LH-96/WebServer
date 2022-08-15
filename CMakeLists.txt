cmake_minimum_required(VERSION 3.0.0)
project(WebServer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 14)

file(GLOB ALL_FILES "src/*.cpp")

add_executable(WebServer ${ALL_FILES})

find_package(Threads)

target_link_libraries(WebServer ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(WebServer PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_subdirectory(test)
