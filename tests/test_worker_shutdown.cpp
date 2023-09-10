#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
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

TEST(test_worker_loop_exits_on_shutdown)
{
    WorkerState state;
    std::atomic<bool> loop_exited{false};

    std::thread worker([&] {
        while (state.is_running())
        {
            state.wait_for_stop(std::chrono::milliseconds(1));
        }
        loop_exited.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    state.stop();
    worker.join();

    ASSERT_TRUE(loop_exited.load());
}

TEST(test_worker_loop_processes_work_before_exit)
{
    WorkerState state;
    std::atomic<int> work_count{0};

    std::thread worker([&] {
        while (state.is_running())
        {
            work_count.fetch_add(1);
            state.wait_for_stop(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    state.stop();
    worker.join();

    ASSERT_TRUE(work_count.load() > 0);
}

TEST(test_shutdown_with_pending_work)
{
    WorkerState state;
    std::atomic<int> pending{5};
    std::atomic<int> processed{0};

    std::thread worker([&] {
        while (state.is_running())
        {
            if (pending.load() > 0)
            {
                processed.fetch_add(1);
                pending.fetch_sub(1);
            }
            state.wait_for_stop(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    state.stop();
    worker.join();

    ASSERT_TRUE(processed.load() > 0);
    ASSERT_TRUE(processed.load() <= 5);
}

TEST(test_rapid_shutdown_cycle)
{
    for (int i = 0; i < 10; ++i)
    {
        WorkerState state;
        std::atomic<bool> exited{false};

        std::thread worker([&] {
            while (state.is_running())
            {
                state.wait_for_stop(std::chrono::milliseconds(1));
            }
            exited.store(true);
        });

        state.stop();
        worker.join();
        ASSERT_TRUE(exited.load());
    }
}

TEST(test_shutdown_during_work_execution)
{
    WorkerState state;
    std::atomic<bool> in_work{false};
    std::atomic<bool> work_completed{false};

    std::thread worker([&] {
        while (state.is_running())
        {
            in_work.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            work_completed.store(true);
            in_work.store(false);
            state.wait_for_stop(std::chrono::milliseconds(1));
        }
    });

    // Wait until the worker is genuinely inside its work section instead of
    // assuming a fixed sleep lands there. in_work is only true during the 5 ms
    // work window, so a timed guess can easily observe the gap between two
    // iterations -- especially on platforms with coarse sleep granularity,
    // where a 2 ms sleep can overshoot well past the window.
    bool observed_in_work = false;
    for (int i = 0; i < 100000000; ++i)
    {
        if (in_work.load())
        {
            observed_in_work = true;
            break;
        }
        std::this_thread::yield();
    }

    state.stop();
    worker.join();

    // Assert only after the worker has been joined. Throwing while the thread
    // is still joinable would run std::thread's destructor during stack
    // unwinding, which calls std::terminate() and aborts the whole suite
    // instead of reporting a clean test failure.
    ASSERT_TRUE(observed_in_work);
    ASSERT_TRUE(work_completed.load());
    ASSERT_TRUE(!state.is_running());
}

TEST(test_multiple_workers_independent_shutdown)
{
    WorkerState state1, state2;
    std::atomic<bool> worker1_exited{false};
    std::atomic<bool> worker2_exited{false};

    std::thread t1([&] {
        while (state1.is_running())
        {
            state1.wait_for_stop(std::chrono::milliseconds(1));
        }
        worker1_exited.store(true);
    });

    std::thread t2([&] {
        while (state2.is_running())
        {
            state2.wait_for_stop(std::chrono::milliseconds(1));
        }
        worker2_exited.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    state1.stop();
    t1.join();
    ASSERT_TRUE(worker1_exited.load());
    ASSERT_TRUE(worker2_exited.load() == false);

    state2.stop();
    t2.join();
    ASSERT_TRUE(worker2_exited.load());
}

int main()
{
    std::cout << "Worker shutdown tests:\n";

    RUN(test_worker_loop_exits_on_shutdown);
    RUN(test_worker_loop_processes_work_before_exit);
    RUN(test_shutdown_with_pending_work);
    RUN(test_rapid_shutdown_cycle);
    RUN(test_shutdown_during_work_execution);
    RUN(test_multiple_workers_independent_shutdown);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
