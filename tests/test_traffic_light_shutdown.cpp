#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <string>
#include "TrafficLight.h"

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

TEST(test_initial_phase_is_red)
{
    TrafficLight tl;
    ASSERT_TRUE(tl.getCurrentPhase() == TrafficLightPhase::red);
}

TEST(test_explicit_shutdown_stops_thread_promptly)
{
    auto start = std::chrono::steady_clock::now();
    {
        TrafficLight tl;
        tl.simulate();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        tl.shutdown();
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    ASSERT_TRUE(elapsed < 1000);
}

TEST(test_destructor_stops_thread_without_explicit_shutdown)
{
    // Regression test: TrafficLight's phase-cycling thread used to loop
    // forever with no way to stop it, so destroying a running TrafficLight
    // without an explicit shutdown() call would hang in the base class's
    // thread join. The destructor must now signal shutdown itself.
    auto start = std::chrono::steady_clock::now();
    {
        TrafficLight tl;
        tl.simulate();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    ASSERT_TRUE(elapsed < 1000);
}

TEST(test_shutdown_is_idempotent)
{
    TrafficLight tl;
    tl.simulate();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    tl.shutdown();
    tl.shutdown();
    tl.shutdown();
}

TEST(test_shutdown_before_simulate_is_safe)
{
    TrafficLight tl;
    tl.shutdown();
    tl.simulate();
    tl.shutdown();
}

int main()
{
    std::cout << "TrafficLight shutdown-safety tests:\n";

    RUN(test_initial_phase_is_red);
    RUN(test_explicit_shutdown_stops_thread_promptly);
    RUN(test_destructor_stops_thread_without_explicit_shutdown);
    RUN(test_shutdown_is_idempotent);
    RUN(test_shutdown_before_simulate_is_safe);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
