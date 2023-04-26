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

TEST(test_initial_state_is_running)
{
    WorkerState state;
    ASSERT_TRUE(state.is_running());
}

TEST(test_stop_sets_not_running)
{
    WorkerState state;
    state.stop();
    ASSERT_TRUE(!state.is_running());
}

TEST(test_stop_is_idempotent)
{
    WorkerState state;
    state.stop();
    state.stop();
    state.stop();
    ASSERT_TRUE(!state.is_running());
}

TEST(test_restart_resumes_running)
{
    WorkerState state;
    state.stop();
    ASSERT_TRUE(!state.is_running());
    state.restart();
    ASSERT_TRUE(state.is_running());
}

TEST(test_wait_for_stop_returns_on_stop)
{
    WorkerState state;
    std::atomic<bool> exited{false};

    std::thread worker([&] {
        while (state.is_running())
        {
            state.wait_for_stop(std::chrono::milliseconds(10));
        }
        exited.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    ASSERT_TRUE(!exited.load());

    state.stop();
    worker.join();
    ASSERT_TRUE(exited.load());
}

TEST(test_wait_for_stop_times_out)
{
    WorkerState state;
    std::atomic<int> iterations{0};

    std::thread worker([&] {
        while (state.is_running() && iterations.load() < 3)
        {
            state.wait_for_stop(std::chrono::milliseconds(5));
            iterations.fetch_add(1);
        }
    });

    worker.join();
    ASSERT_TRUE(iterations.load() >= 3);
}

TEST(test_worker_exits_cleanly_after_stop)
{
    WorkerState state;
    std::atomic<int> work_done{0};

    std::thread worker([&] {
        while (state.is_running())
        {
            work_done.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    state.stop();
    worker.join();

    ASSERT_TRUE(work_done.load() > 0);
}

TEST(test_multiple_workers_stop_together)
{
    WorkerState state;
    const int num_workers = 4;
    std::atomic<int> exited{0};

    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; ++i)
    {
        workers.emplace_back([&] {
            while (state.is_running())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            exited.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    state.stop();

    for (auto &t : workers) t.join();
    ASSERT_EQ(exited.load(), num_workers);
}

TEST(test_stop_visible_across_threads_immediately)
{
    WorkerState state;
    std::atomic<bool> observer_saw_stop{false};
    std::atomic<bool> observer_saw_running_after_stop{false};

    std::thread observer([&] {
        for (int i = 0; i < 1000; ++i)
        {
            if (!state.is_running())
            {
                observer_saw_stop.store(true);
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    state.stop();
    observer.join();

    ASSERT_TRUE(observer_saw_stop.load());
}

TEST(test_memory_ordering_under_contention)
{
    WorkerState state;
    const int num_readers = 8;
    const int iterations_per_reader = 20000;
    std::atomic<int> saw_running{0};
    std::atomic<int> saw_stopped{0};
    std::atomic<int> observations{0};

    std::vector<std::thread> readers;
    for (int r = 0; r < num_readers; ++r)
    {
        readers.emplace_back([&] {
            for (int i = 0; i < iterations_per_reader; ++i)
            {
                if (state.is_running())
                    saw_running.fetch_add(1);
                else
                    saw_stopped.fetch_add(1);
                observations.fetch_add(1);
            }
        });
    }

    // Wait for genuine in-flight progress (rather than a fixed wall-clock
    // delay) before stopping, so the readers are guaranteed to still be
    // mid-loop and observe both states regardless of scheduler speed.
    while (observations.load() < num_readers * iterations_per_reader / 4)
    {
        std::this_thread::yield();
    }
    state.stop();

    for (auto &t : readers) t.join();

    ASSERT_TRUE(saw_running.load() > 0);
    ASSERT_TRUE(saw_stopped.load() > 0);
    ASSERT_TRUE(saw_running.load() + saw_stopped.load() == num_readers * iterations_per_reader);
}

int main()
{
    std::cout << "WorkerState characterization tests:\n";

    RUN(test_initial_state_is_running);
    RUN(test_stop_sets_not_running);
    RUN(test_stop_is_idempotent);
    RUN(test_restart_resumes_running);
    RUN(test_wait_for_stop_returns_on_stop);
    RUN(test_wait_for_stop_times_out);
    RUN(test_worker_exits_cleanly_after_stop);
    RUN(test_multiple_workers_stop_together);
    RUN(test_stop_visible_across_threads_immediately);
    RUN(test_memory_ordering_under_contention);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
