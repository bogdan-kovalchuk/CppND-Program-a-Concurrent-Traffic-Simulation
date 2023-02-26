#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
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

TEST(test_worker_resumes_after_restart)
{
    WorkerState state;
    std::atomic<int> work_count{0};
    // Explicit termination flag: the worker must survive the intermediate
    // stop() (to prove it resumes on restart()), so it cannot exit on
    // is_running() alone. It also must not depend on reaching a specific
    // work_count, since that value is timing-dependent and the loop would
    // spin forever after the final stop() if the target were never reached.
    std::atomic<bool> done{false};

    std::thread worker([&] {
        while (!done.load())
        {
            if (state.is_running())
            {
                work_count.fetch_add(1);
            }
            state.wait_for_stop(std::chrono::milliseconds(5));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    state.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int paused_count = work_count.load();

    state.restart();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    state.stop();
    done.store(true);
    worker.join();

    ASSERT_TRUE(work_count.load() > paused_count);
}

TEST(test_repeated_stop_restart_cycles)
{
    WorkerState state;
    const int cycles = 10;
    std::atomic<int> wake_count{0};

    std::thread worker([&] {
        for (int i = 0; i < cycles; ++i)
        {
            state.wait_for_stop(std::chrono::milliseconds(2000));
            wake_count.fetch_add(1);
        }
    });

    for (int i = 0; i < cycles; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        state.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (i < cycles - 1)
            state.restart();
    }

    worker.join();
    ASSERT_EQ(wake_count.load(), cycles);
}

TEST(test_lifecycle_state_transitions)
{
    WorkerState state;
    ASSERT_TRUE(state.is_running());

    state.stop();
    ASSERT_TRUE(!state.is_running());

    state.restart();
    ASSERT_TRUE(state.is_running());

    state.restart();
    ASSERT_TRUE(state.is_running());

    state.stop();
    ASSERT_TRUE(!state.is_running());

    state.stop();
    ASSERT_TRUE(!state.is_running());

    state.restart();
    ASSERT_TRUE(state.is_running());
}

TEST(test_worker_accumulates_work_across_cycles)
{
    WorkerState state;
    std::atomic<int> total_work{0};
    const int cycles = 5;

    std::thread worker([&] {
        for (int c = 0; c < cycles; ++c)
        {
            while (state.is_running())
            {
                total_work.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            state.wait_for_stop(std::chrono::milliseconds(1000));
        }
    });

    for (int c = 0; c < cycles; ++c)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        state.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (c < cycles - 1)
            state.restart();
    }

    worker.join();
    ASSERT_TRUE(total_work.load() > 0);
}

TEST(test_restart_from_initial_state_is_noop)
{
    WorkerState state;
    ASSERT_TRUE(state.is_running());
    state.restart();
    ASSERT_TRUE(state.is_running());

    std::atomic<bool> worker_ran{false};
    std::thread worker([&] {
        if (state.is_running())
        {
            worker_ran.store(true);
        }
    });
    worker.join();
    ASSERT_TRUE(worker_ran.load());
}

int main()
{
    std::cout << "Worker lifecycle tests:\n";

    RUN(test_worker_resumes_after_restart);
    RUN(test_repeated_stop_restart_cycles);
    RUN(test_lifecycle_state_transitions);
    RUN(test_worker_accumulates_work_across_cycles);
    RUN(test_restart_from_initial_state_is_noop);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
