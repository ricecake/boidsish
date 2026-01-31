#include <gtest/gtest.h>
#include "concurrent_queue.h"
#include <thread>
#include <vector>
#include <atomic>

using namespace Boidsish;

TEST(ConcurrentQueueTest, BasicOps) {
    ConcurrentQueue<int> q;
    EXPECT_TRUE(q.empty());

    q.push(1);
    EXPECT_FALSE(q.empty());

    int val;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(q.empty());
}

TEST(ConcurrentQueueTest, MultiThreaded) {
    ConcurrentQueue<int> q;
    const int num_producers = 4;
    const int num_items_per_producer = 1000;
    std::vector<std::thread> producers;

    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&q, num_items_per_producer, i]() {
            for (int j = 0; j < num_items_per_producer; ++j) {
                q.push(i * num_items_per_producer + j);
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

    EXPECT_EQ(count, num_producers * num_items_per_producer);
}
