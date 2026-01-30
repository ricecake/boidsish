#include <gtest/gtest.h>
#include "thread_pool.h"
#include <chrono>

using namespace Boidsish;

TEST(ThreadPoolTest, EnqueueAndGet) {
    ThreadPool pool;
    auto handle = pool.enqueue(TaskPriority::MEDIUM, []() {
        return 42;
    });

    EXPECT_EQ(handle.get(), 42);
}

TEST(ThreadPoolTest, IsReady) {
    ThreadPool pool;
    auto handle = pool.enqueue(TaskPriority::MEDIUM, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 42;
    });

    EXPECT_FALSE(handle.is_ready());
    // Wait for it to be ready
    for(int i=0; i<100; ++i) {
        if (handle.is_ready()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(handle.is_ready());
    EXPECT_EQ(handle.get(), 42);
}

TEST(ThreadPoolTest, MultipleTasks) {
    ThreadPool pool;
    std::vector<TaskHandle<int>> handles;
    for (int i = 0; i < 10; ++i) {
        handles.push_back(pool.enqueue(TaskPriority::MEDIUM, [i]() {
            return i * i;
        }));
    }

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(handles[i].get(), i * i);
    }
}
