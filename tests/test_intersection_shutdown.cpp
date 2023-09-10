#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <string>
#include "Intersection.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name()
#define RUN(name) do { \
    std::cout << "  " #name "..."; \
    try { name(); std::cout << " PASSED\n"; ++tests_passed; } \
    catch (const std::exception &e) { std::cout << " FAILED: " << e.what() << "\n"; ++tests_failed; } \
    catch (...) { std::cout << " FAILED (unknown)\n"; ++tests_failed; } \
} while(0)

#define ASSERT_TRUE(x) do { if (!(x)) throw std::runtime_error(std::string("ASSERT_TRUE failed at line ") + std::to_string(__LINE__)); } while(0)

TEST(test_shutdown_stops_queue_thread_and_owned_traffic_light_promptly)
{
    // Regression test: Intersection::shutdown() used to stop only its own
    // queue-processing thread, leaving the owned TrafficLight's
    // phase-cycling thread (which used to run forever) unaffected. Both
    // threads must now stop within a bounded time.
    auto start = std::chrono::steady_clock::now();
    {
        Intersection i;
        i.simulate();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        i.shutdown();
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    ASSERT_TRUE(elapsed < 1000);
}

TEST(test_destructor_stops_all_threads_without_explicit_shutdown)
{
    auto start = std::chrono::steady_clock::now();
    {
        Intersection i;
        i.simulate();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        // i goes out of scope here without an explicit shutdown()
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    ASSERT_TRUE(elapsed < 1000);
}

TEST(test_traffic_light_is_green_reflects_current_phase)
{
    Intersection i;
    // The traffic light starts red and has not been simulate()'d, so it
    // cannot have transitioned yet.
    ASSERT_TRUE(!i.trafficLightIsGreen());
}

TEST(test_shutdown_is_idempotent)
{
    Intersection i;
    i.simulate();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    i.shutdown();
    i.shutdown();
    i.shutdown();
}

int main()
{
    std::cout << "Intersection shutdown-safety tests:\n";

    RUN(test_shutdown_stops_queue_thread_and_owned_traffic_light_promptly);
    RUN(test_destructor_stops_all_threads_without_explicit_shutdown);
    RUN(test_traffic_light_is_green_reflects_current_phase);
    RUN(test_shutdown_is_idempotent);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
