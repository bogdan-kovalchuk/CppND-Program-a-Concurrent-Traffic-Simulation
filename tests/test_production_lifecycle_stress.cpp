#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

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

TEST(test_repeated_concurrent_shutdown_of_live_objects)
{
    constexpr int iterations = 30;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);

    for (int iteration = 0; iteration < iterations; ++iteration)
    {
        auto start = std::make_shared<Intersection>();
        auto destination = std::make_shared<Intersection>();
        start->setPosition(0.0, 0.0);
        destination->setPosition(1000.0, 0.0);

        auto street = std::make_shared<Street>();
        street->setInIntersection(start);
        street->setOutIntersection(destination);

        auto vehicle = std::make_shared<Vehicle>();
        vehicle->setCurrentStreet(street);
        vehicle->setCurrentDestination(destination);

        destination->simulate();
        vehicle->simulate();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        std::thread first_stopper([&] {
            destination->shutdown();
            vehicle->shutdown();
        });
        std::thread second_stopper([&] {
            vehicle->shutdown();
            destination->shutdown();
        });
        first_stopper.join();
        second_stopper.join();

        vehicle.reset();
        ASSERT_TRUE(std::chrono::steady_clock::now() < deadline);
    }
}

int main()
{
    std::cout << "Production lifecycle stress tests:\n";
    RUN(test_repeated_concurrent_shutdown_of_live_objects);
    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed == 0 ? 0 : 1;
}
