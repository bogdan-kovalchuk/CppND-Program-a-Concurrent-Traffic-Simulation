#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <string>
#include <algorithm>
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

TEST(test_fifo_preserves_insertion_order)
{
    FifoMessageQueue<int> q;
    for (int i = 0; i < 5; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_EQ(q.receive(), i);
    }
}

TEST(test_lifo_reverses_insertion_order)
{
    MessageQueue<int> q;
    for (int i = 0; i < 5; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }
    for (int i = 4; i >= 0; --i)
    {
        ASSERT_EQ(q.receive(), i);
    }
}

TEST(test_fifo_concurrent_ordering)
{
    FifoMessageQueue<int> q;
    const int n = 200;
    std::atomic<int> send_count{0};

    std::thread producer([&] {
        for (int i = 0; i < n; ++i)
        {
            int v = i;
            q.send(std::move(v));
            send_count.fetch_add(1);
        }
    });

    std::vector<int> received;
    received.reserve(n);
    std::thread consumer([&] {
        for (int i = 0; i < n; ++i)
        {
            received.push_back(q.receive());
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(static_cast<int>(received.size()), n);
    for (int i = 0; i < n; ++i)
    {
        ASSERT_EQ(received[i], i);
    }
}

TEST(test_both_queues_handle_same_throughput)
{
    const int n = 1000;
    const int num_producers = 4;

    auto benchmark = [&](auto &queue) {
        std::atomic<int> total_sent{0};
        std::vector<std::thread> producers;
        for (int p = 0; p < num_producers; ++p)
        {
            producers.emplace_back([&, p] {
                for (int i = 0; i < n; ++i)
                {
                    int v = p * n + i;
                    queue.send(std::move(v));
                    total_sent.fetch_add(1);
                }
            });
        }

        std::atomic<int> total_recv{0};
        std::thread consumer([&] {
            while (total_recv.load() < num_producers * n)
            {
                queue.receive();
                total_recv.fetch_add(1);
            }
        });

        for (auto &t : producers) t.join();
        consumer.join();
        ASSERT_EQ(total_sent.load(), num_producers * n);
        ASSERT_EQ(total_recv.load(), num_producers * n);
    };

    MessageQueue<int> lifo_q;
    benchmark(lifo_q);

    FifoMessageQueue<int> fifo_q;
    benchmark(fifo_q);
}

TEST(test_fifo_multi_producer_all_delivered)
{
    FifoMessageQueue<int> q;
    const int per_producer = 50;
    const int num_producers = 8;
    const int total = per_producer * num_producers;

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p)
    {
        producers.emplace_back([&, p] {
            for (int i = 0; i < per_producer; ++i)
            {
                int v = p * per_producer + i;
                q.send(std::move(v));
            }
        });
    }
    for (auto &t : producers) t.join();

    ASSERT_EQ(q.size(), static_cast<std::size_t>(total));

    std::vector<bool> seen(total, false);
    for (int i = 0; i < total; ++i)
    {
        int v = q.receive();
        ASSERT_TRUE(v >= 0 && v < total);
        seen[v] = true;
    }
    for (int i = 0; i < total; ++i)
    {
        ASSERT_TRUE(seen[i]);
    }
}

int main()
{
    std::cout << "Queue comparison tests:\n";

    RUN(test_fifo_preserves_insertion_order);
    RUN(test_lifo_reverses_insertion_order);
    RUN(test_fifo_concurrent_ordering);
    RUN(test_both_queues_handle_same_throughput);
    RUN(test_fifo_multi_producer_all_delivered);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
