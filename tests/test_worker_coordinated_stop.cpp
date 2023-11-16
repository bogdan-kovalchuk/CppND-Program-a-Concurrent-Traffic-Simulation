#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <barrier>
#include "WorkerState.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name()
#define RUN(name) do { \
    std::cout << "  " #name "..."; \
    try { name(); std::cout << " PASSED\n"; ++tests_passed; } \
    catch (const std::exception &e) { std::cout << " FAILED: " << e.what() << "\n"; ++tests_failed; } \
    catch (...) { std::cout << " FAILED (unknown)\n"; ++tests_failed; } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
        throw std::runtime_error(std::string("ASSERT_EQ failed: ") + std::to_string(_a) + " != " + std::to_string(_b) + " at line " + std::to_string(__LINE__)); \
    } \
} while(0)

#define ASSERT_TRUE(x) do { if (!(x)) throw std::runtime_error(std::string("ASSERT_TRUE failed at line ") + std::to_string(__LINE__)); } while(0)

TEST(test_all_workers_stop_within_deadline)
{
    WorkerState state;
    const int num_workers = 8;
    std::atomic<int> stopped{0};
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);

    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; ++i)
    {
        workers.emplace_back([&] {
            while (state.is_running())
            {
                state.wait_for_stop(std::chrono::milliseconds(5));
            }
            stopped.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    state.stop();

    for (auto &t : workers) t.join();
    auto now = std::chrono::steady_clock::now();

    ASSERT_EQ(stopped.load(), num_workers);
    ASSERT_TRUE(now < deadline);
}

TEST(test_phased_shutdown_with_worker_groups)
{
    WorkerState state_a, state_b;
    const int group_size = 4;
    std::atomic<int> group_a_stopped{0};
    std::atomic<int> group_b_stopped{0};

    std::vector<std::thread> group_a;
    for (int i = 0; i < group_size; ++i)
    {
        group_a.emplace_back([&] {
            while (state_a.is_running())
            {
                state_a.wait_for_stop(std::chrono::milliseconds(2));
            }
            group_a_stopped.fetch_add(1);
        });
    }

    std::vector<std::thread> group_b;
    for (int i = 0; i < group_size; ++i)
    {
        group_b.emplace_back([&] {
            while (state_b.is_running())
            {
                state_b.wait_for_stop(std::chrono::milliseconds(2));
            }
            group_b_stopped.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    state_a.stop();

    for (auto &t : group_a) t.join();
    ASSERT_EQ(group_a_stopped.load(), group_size);
    ASSERT_EQ(group_b_stopped.load(), 0);

    state_b.stop();
    for (auto &t : group_b) t.join();
    ASSERT_EQ(group_b_stopped.load(), group_size);
}

TEST(test_workers_detect_stop_without_lost_wakeup)
{
    WorkerState state;
    const int num_workers = 6;
    std::atomic<int> detected{0};

    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; ++i)
    {
        workers.emplace_back([&] {
            while (!state.is_running() == false)
            {
                state.wait_for_stop(std::chrono::milliseconds(100));
                if (!state.is_running())
                {
                    detected.fetch_add(1);
                    break;
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    state.stop();

    for (auto &t : workers) t.join();
    ASSERT_EQ(detected.load(), num_workers);
}

TEST(test_shared_state_across_dynamic_worker_pool)
{
    WorkerState state;
    std::atomic<int> total_work{0};
    const int waves = 3;
    const int workers_per_wave = 4;

    for (int w = 0; w < waves; ++w)
    {
        state.restart();
        std::vector<std::thread> workers;
        for (int i = 0; i < workers_per_wave; ++i)
        {
            workers.emplace_back([&] {
                while (state.is_running())
                {
                    total_work.fetch_add(1);
                    state.wait_for_stop(std::chrono::milliseconds(2));
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        state.stop();
        for (auto &t : workers) t.join();
    }

    ASSERT_TRUE(total_work.load() > 0);
}

int main()
{
    std::cout << "Coordinated multi-worker stop tests:\n";

    RUN(test_all_workers_stop_within_deadline);
    RUN(test_phased_shutdown_with_worker_groups);
    RUN(test_workers_detect_stop_without_lost_wakeup);
    RUN(test_shared_state_across_dynamic_worker_pool);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
