cmake_minimum_required(VERSION 3.0)
project(smt_test)

set(CMAKE_CXX_FLAGS "-std=c++17 -pthread -ldl -Wl,--no-as-needed -O3 -g -shared-libgcc -march=native")

list(APPEND CMAKE_MODULE_PATH "cmake")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(TBB REQUIRED)

add_executable(smt_test test.cpp)
target_link_libraries(smt_test Threads::Threads tbb)