#include <iostream>
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

#define ASSERT_THROWS(expr, exception_type) do { \
    bool threw = false; \
    try { expr; } \
    catch (const exception_type &) { threw = true; } \
    if (!threw) throw std::runtime_error(std::string("expected " #exception_type " at line ") + std::to_string(__LINE__)); \
} while(0)

TEST(test_street_set_in_intersection_rejects_null)
{
    auto street = std::make_shared<Street>();
    ASSERT_THROWS(street->setInIntersection(nullptr), std::invalid_argument);
}

TEST(test_street_set_out_intersection_rejects_null)
{
    auto street = std::make_shared<Street>();
    ASSERT_THROWS(street->setOutIntersection(nullptr), std::invalid_argument);
}

TEST(test_street_set_in_intersection_accepts_valid_pointer)
{
    auto street = std::make_shared<Street>();
    auto intersection = std::make_shared<Intersection>();
    street->setInIntersection(intersection);
    ASSERT_TRUE(street->getInIntersection() == intersection);
}

TEST(test_vehicle_set_current_destination_rejects_null)
{
    Vehicle vehicle;
    ASSERT_THROWS(vehicle.setCurrentDestination(nullptr), std::invalid_argument);
}

TEST(test_vehicle_set_current_destination_accepts_valid_pointer)
{
    Vehicle vehicle;
    auto intersection = std::make_shared<Intersection>();
    vehicle.setCurrentDestination(intersection);
}

TEST(test_intersection_add_street_rejects_null)
{
    Intersection intersection;
    ASSERT_THROWS(intersection.addStreet(nullptr), std::invalid_argument);
}

TEST(test_intersection_query_streets_rejects_null)
{
    Intersection intersection;
    ASSERT_THROWS(intersection.queryStreets(nullptr), std::invalid_argument);
}

TEST(test_intersection_add_street_accepts_valid_pointer)
{
    auto intersection = std::make_shared<Intersection>();
    auto street = std::make_shared<Street>();
    intersection->addStreet(street);

    // Query using a different street as the caller's own street; the added
    // street should show up as an outgoing option.
    auto otherStreet = std::make_shared<Street>();
    auto outgoing = intersection->queryStreets(otherStreet);
    ASSERT_TRUE(outgoing.size() == 1);
}

int main()
{
    std::cout << "Input validation tests:\n";

    RUN(test_street_set_in_intersection_rejects_null);
    RUN(test_street_set_out_intersection_rejects_null);
    RUN(test_street_set_in_intersection_accepts_valid_pointer);
    RUN(test_vehicle_set_current_destination_rejects_null);
    RUN(test_vehicle_set_current_destination_accepts_valid_pointer);
    RUN(test_intersection_add_street_rejects_null);
    RUN(test_intersection_add_street_accepts_valid_pointer);
    RUN(test_intersection_query_streets_rejects_null);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
