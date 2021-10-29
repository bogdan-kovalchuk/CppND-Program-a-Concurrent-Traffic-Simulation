cmake_minimum_required(VERSION 3.11.3)
project(traffic_sim_tests)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread -Wall -Wextra -Wpedantic")

include_directories(${CMAKE_SOURCE_DIR}/../src)

find_package(Threads REQUIRED)

add_executable(test_message_queue test_message_queue.cpp)
target_link_libraries(test_message_queue Threads::Threads)

add_executable(test_queue_comparison test_queue_comparison.cpp)
target_link_libraries(test_queue_comparison Threads::Threads)

add_executable(test_queue_hardening test_queue_hardening.cpp)
target_link_libraries(test_queue_hardening Threads::Threads)
