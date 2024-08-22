#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "Intersection.h"
#include "Street.h"

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

TEST(test_street_queries_are_safe_during_registration)
{
    Intersection intersection;
    auto incoming = std::make_shared<Street>();
    constexpr int street_count = 200;
    std::atomic<bool> registration_done{false};
    std::atomic<bool> valid_results{true};

    std::thread writer([&] {
        for (int index = 0; index < street_count; ++index)
            intersection.addStreet(std::make_shared<Street>());
        registration_done.store(true);
    });

    std::thread reader([&] {
        while (!registration_done.load())
        {
            const auto streets = intersection.queryStreets(incoming);
            if (streets.size() > street_count)
                valid_results.store(false);
            for (const auto &street : streets)
            {
                if (!street)
                    valid_results.store(false);
            }
        }
    });

    writer.join();
    reader.join();
    const auto streets = intersection.queryStreets(incoming);
    ASSERT_TRUE(valid_results.load());
    ASSERT_TRUE(streets.size() == street_count);
}

int main()
{
    std::cout << "Intersection concurrency tests:\n";
    RUN(test_street_queries_are_safe_during_registration);
    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed == 0 ? 0 : 1;
}
