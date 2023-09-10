#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include "Street.h"
#include "Intersection.h"
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

// Builds a two-intersection, one-street graph long enough that a vehicle
// driving at the default speed cannot reach the intersection-entry
// threshold (90% of the street) within the short windows these tests run
// for, so the drive thread stays in its plain position-update loop where
// shutdown() is exercised.
static std::shared_ptr<Vehicle> makeRoamingVehicle(std::shared_ptr<Intersection> &i1, std::shared_ptr<Intersection> &i2, std::shared_ptr<Street> &street)
{
    i1 = std::make_shared<Intersection>();
    i2 = std::make_shared<Intersection>();
    i1->setPosition(0, 0);
    i2->setPosition(1000, 0);
    street = std::make_shared<Street>();
    street->setInIntersection(i1);
    street->setOutIntersection(i2);

    auto vehicle = std::make_shared<Vehicle>();
    vehicle->setCurrentStreet(street);
    vehicle->setCurrentDestination(i2);
    return vehicle;
}

TEST(test_explicit_shutdown_stops_thread_promptly)
{
    std::shared_ptr<Intersection> i1, i2;
    std::shared_ptr<Street> street;
    auto vehicle = makeRoamingVehicle(i1, i2, street);

    auto start = std::chrono::steady_clock::now();
    vehicle->simulate();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    vehicle->shutdown();
    vehicle.reset();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    ASSERT_TRUE(elapsed < 1000);
}

TEST(test_destructor_stops_thread_without_explicit_shutdown)
{
    // Regression test: Vehicle::drive() used to loop forever with no way to
    // stop it, so destroying a running Vehicle without an explicit
    // shutdown() call would hang in the base class's thread join.
    auto start = std::chrono::steady_clock::now();
    {
        std::shared_ptr<Intersection> i1, i2;
        std::shared_ptr<Street> street;
        auto vehicle = makeRoamingVehicle(i1, i2, street);
        vehicle->simulate();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        // vehicle goes out of scope here without an explicit shutdown()
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    ASSERT_TRUE(elapsed < 1000);
}

TEST(test_shutdown_is_idempotent)
{
    std::shared_ptr<Intersection> i1, i2;
    std::shared_ptr<Street> street;
    auto vehicle = makeRoamingVehicle(i1, i2, street);

    vehicle->simulate();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    vehicle->shutdown();
    vehicle->shutdown();
    vehicle->shutdown();
}

int main()
{
    std::cout << "Vehicle shutdown-safety tests:\n";

    RUN(test_explicit_shutdown_stops_thread_promptly);
    RUN(test_destructor_stops_thread_without_explicit_shutdown);
    RUN(test_shutdown_is_idempotent);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
