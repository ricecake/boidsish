#include <gtest/gtest.h>
#include "pool.h"
#include <string>
#include <memory>

using namespace Boidsish;

TEST(PoolTest, BasicAllocation) {
    Pool<int> pool;
    auto h1 = pool.Allocate(10);
    auto h2 = pool.Allocate(20);

    EXPECT_TRUE(h1.IsValid());
    EXPECT_TRUE(h2.IsValid());
    EXPECT_EQ(*h1, 10);
    EXPECT_EQ(*h2, 20);
    EXPECT_EQ(pool.Size(), 2);
}

TEST(PoolTest, FreeAndReuse) {
    Pool<int> pool;
    auto h1 = pool.Allocate(10);
    uint32_t id1 = h1.GetId();

    pool.Free(h1);
    EXPECT_FALSE(h1.IsValid());
    EXPECT_FALSE(pool.IsValid(id1));
    EXPECT_EQ(pool.Size(), 0);

    auto h2 = pool.Allocate(20);
    EXPECT_TRUE(h2.IsValid());
    EXPECT_EQ(*h2, 20);
    EXPECT_EQ(pool.Size(), 1);
    // Generation should have incremented, so id1 is still invalid
    EXPECT_FALSE(pool.IsValid(id1));
}

TEST(PoolTest, Iteration) {
    Pool<int> pool;
    pool.Allocate(1);
    pool.Allocate(2);
    pool.Allocate(3);

    int sum = 0;
    pool.ForEach([&sum](int val) {
        sum += val;
    });
    EXPECT_EQ(sum, 6);
}

TEST(PoolTest, ComplexType) {
    Pool<std::string> pool;
    auto h1 = pool.Allocate("hello");
    auto h2 = pool.Allocate("world");

    EXPECT_EQ(*h1, "hello");
    EXPECT_EQ(*h2, "world");

    pool.Free(h1);
    auto h3 = pool.Allocate("reused");
    EXPECT_EQ(*h3, "reused");
    EXPECT_EQ(pool.Size(), 2);
}

TEST(PoolTest, GetAsShared) {
    Pool<int> pool;
    auto h1 = pool.Allocate(42);
    auto shared = pool.GetAsShared(h1.GetId());

    EXPECT_NE(shared, nullptr);
    EXPECT_EQ(*shared, 42);

    // Changing through shared_ptr affects pool
    *shared = 100;
    EXPECT_EQ(*h1, 100);
}
