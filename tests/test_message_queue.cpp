#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <string>
#include "MessageQueue.h"

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

TEST(test_send_receive_single)
{
    MessageQueue<int> q;
    int val = 42;
    q.send(std::move(val));
    int result = q.receive();
    ASSERT_EQ(result, 42);
}

TEST(test_send_receive_multiple_lifo_order)
{
    MessageQueue<int> q;
    for (int i = 0; i < 5; ++i)
    {
        int v = i;
        q.send(std::move(v));
    }
    ASSERT_EQ(q.size(), static_cast<std::size_t>(5));

    int r0 = q.receive();
    int r1 = q.receive();
    ASSERT_EQ(r0, 4);
    ASSERT_EQ(r1, 3);
}

TEST(test_receive_blocks_until_send)
{
    MessageQueue<int> q;
    std::atomic<bool> received{false};
    int received_val = 0;

    std::thread receiver([&] {
        received_val = q.receive();
        received.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(!received.load());

    int val = 99;
    q.send(std::move(val));

    receiver.join();
    ASSERT_TRUE(received.load());
    ASSERT_EQ(received_val, 99);
}

TEST(test_empty_queue)
{
    MessageQueue<int> q;
    ASSERT_TRUE(q.empty());
    ASSERT_EQ(q.size(), static_cast<std::size_t>(0));

    int v = 1;
    q.send(std::move(v));
    ASSERT_TRUE(!q.empty());
    ASSERT_EQ(q.size(), static_cast<std::size_t>(1));

    q.receive();
    ASSERT_TRUE(q.empty());
}

TEST(test_concurrent_producers)
{
    MessageQueue<int> q;
    const int num_producers = 4;
    const int msgs_per_producer = 100;

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p)
    {
        producers.emplace_back([&, p] {
            for (int i = 0; i < msgs_per_producer; ++i)
            {
                int v = p * msgs_per_producer + i;
                q.send(std::move(v));
            }
        });
    }

    for (auto &t : producers) t.join();

    ASSERT_EQ(q.size(), static_cast<std::size_t>(num_producers * msgs_per_producer));

    std::vector<bool> seen(num_producers * msgs_per_producer, false);
    for (int i = 0; i < num_producers * msgs_per_producer; ++i)
    {
        int v = q.receive();
        ASSERT_TRUE(v >= 0 && v < num_producers * msgs_per_producer);
        seen[v] = true;
    }

    for (int i = 0; i < num_producers * msgs_per_producer; ++i)
    {
        ASSERT_TRUE(seen[i]);
    }
    ASSERT_TRUE(q.empty());
}

TEST(test_concurrent_producers_single_consumer)
{
    MessageQueue<int> q;
    const int total_msgs = 500;
    std::atomic<int> consumed{0};

    std::thread producer1([&] {
        for (int i = 0; i < total_msgs / 2; ++i)
        {
            int v = i;
            q.send(std::move(v));
        }
    });

    std::thread producer2([&] {
        for (int i = 0; i < total_msgs / 2; ++i)
        {
            int v = total_msgs / 2 + i;
            q.send(std::move(v));
        }
    });

    std::thread consumer([&] {
        for (int i = 0; i < total_msgs; ++i)
        {
            q.receive();
            consumed.fetch_add(1);
        }
    });

    producer1.join();
    producer2.join();
    consumer.join();

    ASSERT_EQ(consumed.load(), total_msgs);
    ASSERT_TRUE(q.empty());
}

TEST(test_string_messages)
{
    MessageQueue<std::string> q;
    std::string s1 = "hello";
    std::string s2 = "world";
    q.send(std::move(s1));
    q.send(std::move(s2));

    std::string r = q.receive();
    ASSERT_TRUE(r == "world");
    r = q.receive();
    ASSERT_TRUE(r == "hello");
}

int main()
{
    std::cout << "MessageQueue characterization tests:\n";

    RUN(test_send_receive_single);
    RUN(test_send_receive_multiple_lifo_order);
    RUN(test_receive_blocks_until_send);
    RUN(test_empty_queue);
    RUN(test_concurrent_producers);
    RUN(test_concurrent_producers_single_consumer);
    RUN(test_string_messages);

    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed > 0 ? 1 : 0;
}
