#include <iostream>
#include <atomic>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "TrafficObject.h"

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

TEST(test_concurrent_objects_receive_unique_ids)
{
    constexpr int thread_count = 8;
    constexpr int objects_per_thread = 125;
    std::vector<int> ids;
    ids.reserve(thread_count * objects_per_thread);
    std::mutex ids_mutex;
    std::vector<std::thread> threads;

    for (int thread_index = 0; thread_index < thread_count; ++thread_index)
    {
        threads.emplace_back([&] {
            std::vector<int> local_ids;
            local_ids.reserve(objects_per_thread);
            for (int object_index = 0; object_index < objects_per_thread; ++object_index)
            {
                TrafficObject object;
                local_ids.push_back(object.getID());
            }
            std::lock_guard<std::mutex> lock(ids_mutex);
            ids.insert(ids.end(), local_ids.begin(), local_ids.end());
        });
    }

    for (auto &thread : threads)
        thread.join();

    std::set<int> unique_ids(ids.begin(), ids.end());
    ASSERT_TRUE(ids.size() == static_cast<std::size_t>(thread_count * objects_per_thread));
    ASSERT_TRUE(unique_ids.size() == ids.size());
}

TEST(test_position_reads_observe_complete_snapshots)
{
    TrafficObject object;
    std::atomic<bool> writer_done{false};
    std::atomic<bool> consistent{true};

    std::thread writer([&] {
        for (int value = 0; value < 50000; ++value)
            object.setPosition(static_cast<double>(value), static_cast<double>(-value));
        writer_done.store(true);
    });

    std::vector<std::thread> readers;
    for (int index = 0; index < 4; ++index)
    {
        readers.emplace_back([&] {
            while (!writer_done.load())
            {
                double x = 0.0;
                double y = 0.0;
                object.getPosition(x, y);
                if (y != -x)
                    consistent.store(false);
            }
        });
    }

    writer.join();
    for (auto &reader : readers)
        reader.join();
    ASSERT_TRUE(consistent.load());
}

int main()
{
    std::cout << "TrafficObject concurrency tests:\n";
    RUN(test_concurrent_objects_receive_unique_ids);
    RUN(test_position_reads_observe_complete_snapshots);
    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed == 0 ? 0 : 1;
}
