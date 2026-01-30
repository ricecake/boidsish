#include <gtest/gtest.h>
#include "concurrent_queue.h"
#include <thread>
#include <vector>

using namespace Boidsish;

TEST(ConcurrentQueueTest, BasicOperations) {
    ConcurrentQueue<int> q;
    EXPECT_TRUE(q.empty());

    q.push(1);
    EXPECT_FALSE(q.empty());

    int val;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.try_pop(val));
}

TEST(ConcurrentQueueTest, ThreadSafety) {
    ConcurrentQueue<int> q;
    const int num_threads = 10;
    const int items_per_thread = 1000;

    std::vector<std::thread> producers;
    for (int i = 0; i < num_threads; ++i) {
        producers.emplace_back([&q, items_per_thread]() {
            for (int j = 0; j < items_per_thread; ++j) {
                q.push(j);
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    int count = 0;
    int val;
    while (q.try_pop(val)) {
        count++;
    }

    EXPECT_EQ(count, num_threads * items_per_thread);
}
