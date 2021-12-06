#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
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

TEST(test_concurrent_stop_calls)
{
    WorkerState state;
    const int num_stoppers = 8;
    std::atomic<int> stop_count{0};

    std::vector<std::thread> stoppers;
    for (int i = 0; i < num_stoppers; ++i)
    {
        stoppers.emplace_back([&] {
            state.stop();
            stop_count.fetch_add(1);
        });
    }

    for (auto &t : stoppers) t.join();
    ASSERT_EQ(stop_count.load(), num_stoppers);
    ASSERT_TRUE(!state.is_running());
}

TEST(test_stop_before_worker_starts)
{
    WorkerState state;
    state.stop();

    std::atomic<bool> entered_loop{false};
    std::thread worker([&] {
        if (state.is_running())
        {
            entered_loop.store(true);
        }
    });

    worker.join();
    ASSERT_TRUE(!entered_loop.load());
}

TEST(test_rapid_stop_restart_cycles)
{
    WorkerState state;
    const int cycles = 20;
    std::atomic<int> total_work{0};

    std::thread worker([&] {
        for (int i = 0; i < cycles; ++i)
        {
            while (state.is_running())
            {
                total_work.fetch_add(1);
                state.wait_for_stop(std::chrono::milliseconds(1));
            }
            state.wait_for_stop(std::chrono::milliseconds(50));
        }
    });

    for (int i = 0; i < cycles; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        state.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        state.restart();
    }

    state.stop();
    worker.join();
    ASSERT_TRUE(total_work.load() > 0);
}

TEST(test_stop_during_wait_for_stop)
{
    WorkerState state;
    std::atomic<bool> wait_returned{false};

    std::thread worker([&] {
        state.wait_for_stop(std::chrono::milliseconds(1000));
        wait_returned.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    state.stop();
    worker.join();

    ASSERT_TRUE(wait_returned.load());
}

TEST(test_multiple_waiters_all_wake_on_stop)
{
    WorkerState state;
    const int num_waiters = 6;
    std::atomic<int> woke_up{0};

    std::vector<std::thread> waiters;
    for (int i = 0; i < num_waiters; ++i)
    {
        waiters.emplace_back([&] {
            state.wait_for_stop(std::chrono::milliseconds(1000));
            woke_up.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    state.stop();

    for (auto &t : waiters) t.join();
    ASSERT_EQ(woke_up.load(), num_waiters);
}

TEST(test_is_running_consistent_with_stop)
{
    WorkerState state;
    ASSERT_TRUE(state.is_running());

    state.stop();
    ASSERT_TRUE(!state.is_running());

    state.restart();
    ASSERT_TRUE(state.is_running());

    state.stop();
    ASSERT_TRUE(!state.is_running());
}

TEST(test_worker_exits_immediately_if_already_stopped)
{
    WorkerState state;
    state.stop();

    auto start = std::chrono::steady_clock::now();
    std::thread worker([&] {
        while (state.is_running())
        {
            state.wait_for_stop(std::chrono::milliseconds(100));
        }
    });

    worker.join();
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_TRUE(elapsed < 50);
}

int main()
{
    std::cout << "Worker shutdown race edge case tests:\n";

    RUN(test_concurrent_stop_calls);
    RUN(test_stop_before_worker_starts);
    RUN(test_rapid_stop_restart_cycles);
    RUN(test_stop_during_wait_for_stop);
    RUN(test_multiple_waiters_all_wake_on_stop);
    RUN(test_is_running_consistent_with_stop);
    RUN(test_worker_exits_immediately_if_already_stopped);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
