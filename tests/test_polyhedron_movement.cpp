#include <gtest/gtest.h>
#include "polyhedron.h"
#include "entity.h"
#include <glm/gtc/matrix_access.hpp>

namespace Boidsish {

    template <typename ShapeType = Dot>
    class TestEntity : public Entity<ShapeType> {
    public:
        using Entity<ShapeType>::Entity;
        void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
            (void)handler; (void)time; (void)delta_time;
        }
    };

    class PolyhedronIntegrationTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Setup code if needed
        }
    };

    TEST_F(PolyhedronIntegrationTest, MovementViaEntity) {
        // Create an entity with a polyhedron shape
        auto entity = std::make_shared<TestEntity<Polyhedron>>(1, PolyhedronType::Octahedron);

        // Move the entity
        entity->SetPosition(10.0f, 20.0f, 30.0f);
        entity->UpdateShape();

        // Check if the shape's model matrix reflects the position
        auto shape = entity->GetShape();
        glm::mat4 model = shape->GetModelMatrix();

        EXPECT_FLOAT_EQ(model[3][0], 10.0f);
        EXPECT_FLOAT_EQ(model[3][1], 20.0f);
        EXPECT_FLOAT_EQ(model[3][2], 30.0f);
    }

    TEST_F(PolyhedronIntegrationTest, ScalingViaEntity) {
        // Create an entity with a polyhedron shape
        auto entity = std::make_shared<TestEntity<Polyhedron>>(1, PolyhedronType::Octahedron);

        // Scale the entity
        entity->SetSize(5.0f);
        entity->UpdateShape();

        // Check if the shape's model matrix reflects the scale
        auto shape = entity->GetShape();
        glm::mat4 model = shape->GetModelMatrix();

        // Polyhedron should have GetSizeMultiplier() == 1.0f
        // Default scale is 1.0f
        // So model[0][0] should be 1.0 * 5.0 * 1.0 = 5.0
        EXPECT_FLOAT_EQ(shape->GetSize(), 5.0f);
        EXPECT_FLOAT_EQ(model[0][0], 5.0f);
        EXPECT_FLOAT_EQ(model[1][1], 5.0f);
        EXPECT_FLOAT_EQ(model[2][2], 5.0f);
    }

} // namespace Boidsish
