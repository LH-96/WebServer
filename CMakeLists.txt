cmake_minimum_required(VERSION 3.0.0)
project(WebServer VERSION 0.1.0)

include(CTest)
enable_testing()

set(SOURCES
    src/webserver.cpp
    src/main.cpp
)

add_executable(WebServer ${SOURCES})

set(CMAKE_CXX_STANDARD 11)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

find_package(Threads) # 线程库

target_link_libraries(WebServer ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(WebServer PRIVATE ${PROJECT_SOURCE_DIR}/include)
