#include <gtest/gtest.h>
#include "thread_pool.h"
#include <atomic>

using namespace Boidsish;

TEST(ThreadPoolTest, EnqueueAndGet) {
    ThreadPool pool;
    auto handle = pool.enqueue(TaskPriority::MEDIUM, []() {
        return 42;
    });
    EXPECT_EQ(handle.get(), 42);
}

TEST(ThreadPoolTest, Priorities) {
    ThreadPool pool;
    std::atomic<int> counter = 0;

    // Enqueue multiple tasks with different priorities
    auto h1 = pool.enqueue(TaskPriority::LOW, [&]() { counter++; return 1; });
    auto h2 = pool.enqueue(TaskPriority::HIGH, [&]() { counter++; return 2; });
    auto h3 = pool.enqueue(TaskPriority::MEDIUM, [&]() { counter++; return 3; });

    EXPECT_EQ(h1.get(), 1);
    EXPECT_EQ(h2.get(), 2);
    EXPECT_EQ(h3.get(), 3);
    EXPECT_EQ(counter.load(), 3);
}
