cmake_minimum_required(VERSION 3.11.3)
project(traffic_sim_tests)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread -Wall -Wextra -Wpedantic")

enable_testing()

include_directories(${CMAKE_SOURCE_DIR}/../src)

find_package(Threads REQUIRED)

add_executable(test_message_queue test_message_queue.cpp)
target_link_libraries(test_message_queue Threads::Threads)
add_test(NAME message_queue COMMAND test_message_queue)

add_executable(test_queue_comparison test_queue_comparison.cpp)
target_link_libraries(test_queue_comparison Threads::Threads)
add_test(NAME queue_comparison COMMAND test_queue_comparison)

add_executable(test_queue_hardening test_queue_hardening.cpp)
target_link_libraries(test_queue_hardening Threads::Threads)
add_test(NAME queue_hardening COMMAND test_queue_hardening)

add_executable(test_queue_sync test_queue_sync.cpp)
target_link_libraries(test_queue_sync Threads::Threads)
add_test(NAME queue_sync COMMAND test_queue_sync)

add_executable(test_shutdown_drain test_shutdown_drain.cpp)
target_link_libraries(test_shutdown_drain Threads::Threads)
add_test(NAME shutdown_drain COMMAND test_shutdown_drain)

add_executable(test_queue_backpressure test_queue_backpressure.cpp)
target_link_libraries(test_queue_backpressure Threads::Threads)
add_test(NAME queue_backpressure COMMAND test_queue_backpressure)

add_executable(test_worker_state test_worker_state.cpp)
target_link_libraries(test_worker_state Threads::Threads)
add_test(NAME worker_state COMMAND test_worker_state)

add_executable(test_worker_shutdown test_worker_shutdown.cpp)
target_link_libraries(test_worker_shutdown Threads::Threads)
add_test(NAME worker_shutdown COMMAND test_worker_shutdown)

add_executable(test_worker_race_edge_cases test_worker_race_edge_cases.cpp)
target_link_libraries(test_worker_race_edge_cases Threads::Threads)
add_test(NAME worker_race_edge_cases COMMAND test_worker_race_edge_cases)

add_executable(test_worker_lifecycle test_worker_lifecycle.cpp)
target_link_libraries(test_worker_lifecycle Threads::Threads)
add_test(NAME worker_lifecycle COMMAND test_worker_lifecycle)

add_executable(test_worker_coordinated_stop test_worker_coordinated_stop.cpp)
target_link_libraries(test_worker_coordinated_stop Threads::Threads)
add_test(NAME worker_coordinated_stop COMMAND test_worker_coordinated_stop)
