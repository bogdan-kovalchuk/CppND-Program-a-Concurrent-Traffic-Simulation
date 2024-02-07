#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <set>
#include "MessageQueue.h"
#include "FifoMessageQueue.h"

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

TEST(test_drain_is_atomic_with_respect_to_size)
{
    FifoMessageQueue<int> q;
    const int n = 100;
    for (int i = 0; i < n; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }

    ASSERT_EQ(q.size(), static_cast<std::size_t>(n));

    auto batch = q.drain();
    ASSERT_EQ(static_cast<int>(batch.size()), n);
    ASSERT_EQ(q.size(), static_cast<std::size_t>(0));
    ASSERT_TRUE(q.empty());
}

TEST(test_drain_then_send_works)
{
    MessageQueue<int> q;
    for (int i = 0; i < 5; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }

    auto batch = q.drain();
    ASSERT_EQ(static_cast<int>(batch.size()), 5);
    ASSERT_TRUE(q.empty());

    int v = 99;
    q.send(std::move(v));
    ASSERT_EQ(q.size(), static_cast<std::size_t>(1));

    int result = q.receive();
    ASSERT_EQ(result, 99);
}

TEST(test_concurrent_drain_and_send)
{
    FifoMessageQueue<int> q;
    const int num_senders = 3;
    const int msgs_per_sender = 50;
    std::atomic<int> total_sent{0};

    std::vector<std::thread> senders;
    for (int s = 0; s < num_senders; ++s)
    {
        senders.emplace_back([&] {
            for (int i = 0; i < msgs_per_sender; ++i)
            {
                int v = i;
                if (q.try_send(std::move(v)))
                    total_sent.fetch_add(1);
            }
        });
    }

    for (auto &t : senders) t.join();

    auto batch = q.drain();
    ASSERT_EQ(static_cast<int>(batch.size()), total_sent.load());
    ASSERT_TRUE(q.empty());
}

TEST(test_multiple_drains_exhaust_queue)
{
    MessageQueue<int> q;
    const int n = 30;
    for (int i = 0; i < n; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }

    int total_drained = 0;
    for (int round = 0; round < 5; ++round)
    {
        auto batch = q.drain();
        total_drained += static_cast<int>(batch.size());
    }

    ASSERT_EQ(total_drained, n);
    ASSERT_TRUE(q.empty());
}

TEST(test_drain_preserves_all_elements)
{
    FifoMessageQueue<int> q;
    const int n = 20;
    std::set<int> sent;
    for (int i = 0; i < n; ++i)
    {
        int v = i;
        q.send(std::move(v));
        sent.insert(v);
    }

    auto batch = q.drain();
    ASSERT_EQ(static_cast<int>(batch.size()), n);

    std::set<int> received(batch.begin(), batch.end());
    ASSERT_EQ(static_cast<int>(received.size()), n);
    ASSERT_TRUE(received == sent);
}

int main()
{
    std::cout << "Queue drain consistency tests:\n";

    RUN(test_drain_is_atomic_with_respect_to_size);
    RUN(test_drain_then_send_works);
    RUN(test_concurrent_drain_and_send);
    RUN(test_multiple_drains_exhaust_queue);
    RUN(test_drain_preserves_all_elements);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
