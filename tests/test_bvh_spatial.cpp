#include <gtest/gtest.h>
#include "spatial_entity_handler.h"
#include "task_thread_pool.hpp"
#include <memory>

using namespace Boidsish;

class TestEntity : public Entity<> {
public:
    TestEntity(int id, const Vector3& pos) : Entity<>(id) { SetPosition(pos); }
    void UpdateEntity(const EntityHandler&, float, float) override {}
};

class OtherEntity : public Entity<> {
public:
    OtherEntity(int id, const Vector3& pos) : Entity<>(id) { SetPosition(pos); }
    void UpdateEntity(const EntityHandler&, float, float) override {}
};

TEST(SpatialEntityHandlerTest, RadiusSearch) {
    task_thread_pool::task_thread_pool pool;
    SpatialEntityHandler handler(pool);

    handler.AddEntity<TestEntity>(Vector3(0, 0, 0));
    handler.AddEntity<TestEntity>(Vector3(10, 0, 0));
    handler.AddEntity<OtherEntity>(Vector3(2, 0, 0));

    // Spatial structures are updated in PostTimestep
    handler.operator()(1.0f);

    auto near_entities = handler.GetEntitiesInRadius<TestEntity>(Vector3(0, 0, 0), 5.0f);
    ASSERT_EQ(near_entities.size(), 1);
    EXPECT_EQ(near_entities[0]->GetPosition().x, 0.0f);

    auto all_near = handler.GetEntitiesInRadius<EntityBase>(Vector3(0, 0, 0), 5.0f);
    EXPECT_EQ(all_near.size(), 2);
}

TEST(SpatialEntityHandlerTest, FindNearest) {
    task_thread_pool::task_thread_pool pool;
    SpatialEntityHandler handler(pool);

    handler.AddEntity<TestEntity>(Vector3(0, 0, 0));
    handler.AddEntity<TestEntity>(Vector3(10, 0, 0));
    handler.AddEntity<OtherEntity>(Vector3(2, 0, 0));

    handler.operator()(1.0f);

    auto nearest_test = handler.FindNearest<TestEntity>(Vector3(1, 0, 0));
    ASSERT_NE(nearest_test, nullptr);
    EXPECT_EQ(nearest_test->GetPosition().x, 0.0f);

    auto nearest_any = handler.FindNearest<EntityBase>(Vector3(1, 0, 0));
    ASSERT_NE(nearest_any, nullptr);
    EXPECT_EQ(nearest_any->GetPosition().x, 2.0f);
}

TEST(SpatialEntityHandlerTest, Raycast) {
    task_thread_pool::task_thread_pool pool;
    SpatialEntityHandler handler(pool);

    auto id = handler.AddEntity<TestEntity>(Vector3(10, 0, 0));
    auto entity = handler.GetEntity(id);
    entity->SetSize(2.0f); // Sphere radius 1.0

    handler.operator()(1.0f);

    Ray ray(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0));
    float t;
    glm::vec3 hit_point;
    auto hit = handler.RaycastEntities(ray, t, hit_point);

    ASSERT_NE(hit, nullptr);
    EXPECT_EQ(hit->GetId(), id);
    EXPECT_NEAR(t, 9.0f, 0.1f);
}
