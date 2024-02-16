#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

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

#define ASSERT_TRUE(x) do { if (!(x)) throw std::runtime_error(std::string("ASSERT_TRUE failed at line ") + std::to_string(__LINE__)); } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) throw std::runtime_error(std::string("ASSERT_EQ failed at line ") + std::to_string(__LINE__)); } while(0)

TEST(test_message_queue_blocks_sender_at_capacity)
{
    MessageQueue<int> queue(1);
    int first = 1;
    queue.send(std::move(first));

    std::atomic<bool> sender_started{false};
    std::atomic<bool> sender_finished{false};
    std::thread sender([&] {
        sender_started.store(true);
        int second = 2;
        queue.send(std::move(second));
        sender_finished.store(true);
    });

    while (!sender_started.load())
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const bool sender_blocked = !sender_finished.load();

    ASSERT_EQ(queue.receive(), 1);
    sender.join();
    ASSERT_TRUE(sender_blocked);
    ASSERT_TRUE(sender_finished.load());
    ASSERT_EQ(queue.receive(), 2);
}

TEST(test_message_queue_try_send_rejects_full_queue)
{
    MessageQueue<int> queue(1);
    int first = 1;
    queue.send(std::move(first));
    int second = 2;
    ASSERT_TRUE(!queue.try_send(std::move(second)));
    ASSERT_EQ(queue.receive(), 1);
}

TEST(test_message_queue_rejects_zero_capacity)
{
    bool threw = false;
    try
    {
        MessageQueue<int> queue(0);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(test_fifo_queue_blocks_sender_at_capacity)
{
    FifoMessageQueue<int> queue(1);
    int first = 1;
    queue.send(std::move(first));

    std::atomic<bool> sender_started{false};
    std::atomic<bool> sender_finished{false};
    std::thread sender([&] {
        sender_started.store(true);
        int second = 2;
        queue.send(std::move(second));
        sender_finished.store(true);
    });

    while (!sender_started.load())
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const bool sender_blocked = !sender_finished.load();

    ASSERT_EQ(queue.receive(), 1);
    sender.join();
    ASSERT_TRUE(sender_blocked);
    ASSERT_EQ(queue.receive(), 2);
}

TEST(test_fifo_queue_try_send_rejects_full_queue)
{
    FifoMessageQueue<int> queue(1);
    int first = 1;
    queue.send(std::move(first));
    int second = 2;
    ASSERT_TRUE(!queue.try_send(std::move(second)));
    ASSERT_EQ(queue.receive(), 1);
}

int main()
{
    std::cout << "Bounded MessageQueue tests:\n";
    RUN(test_message_queue_blocks_sender_at_capacity);
    RUN(test_message_queue_try_send_rejects_full_queue);
    RUN(test_message_queue_rejects_zero_capacity);
    RUN(test_fifo_queue_blocks_sender_at_capacity);
    RUN(test_fifo_queue_try_send_rejects_full_queue);
    std::cout << "\nResults: " << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed == 0 ? 0 : 1;
}
