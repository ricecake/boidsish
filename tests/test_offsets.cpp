#include <gtest/gtest.h>
#include "collision.h"
#include "shape.h"
#include "dot.h"
#include "arrow.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

TEST(OffsetTest, AABBTranslate) {
    AABB aabb(glm::vec3(-1.0f), glm::vec3(1.0f));
    AABB translated = aabb.Translate(glm::vec3(5.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(translated.min.x, 4.0f);
    EXPECT_FLOAT_EQ(translated.max.x, 6.0f);
    EXPECT_FLOAT_EQ(translated.min.y, -1.0f);
    EXPECT_FLOAT_EQ(translated.max.y, 1.0f);
}

TEST(OffsetTest, ShapeOffsets) {
    Dot dot(1, 0, 0, 0, 100.0f); // radius 1.0
    dot.SetModelOffset(glm::vec3(0, 0, -2.0f));

    // Model matrix should include offset
    glm::mat4 model = dot.GetModelMatrix();
    glm::vec3 pos = glm::vec3(model[3]);
    EXPECT_FLOAT_EQ(pos.z, -2.0f);

    // Local AABB should NOT include model_offset_
    AABB local = dot.GetLocalAABB();
    EXPECT_FLOAT_EQ(local.min.z, -1.0f);
    EXPECT_FLOAT_EQ(local.max.z, 1.0f);

    // World AABB SHOULD include model_offset_
    AABB world = dot.GetAABB();
    EXPECT_FLOAT_EQ(world.min.z, -3.0f);
    EXPECT_FLOAT_EQ(world.max.z, -1.0f);
}

TEST(OffsetTest, TrailAttachmentPoint) {
    Dot dot(1, 0, 0, 0, 100.0f); // radius 1.0

    // Default attachment: center-back of AABB
    // AABB is [-1, 1], center is 0, back is z=1
    glm::vec3 attachment = dot.GetTrailAttachmentPoint();
    EXPECT_FLOAT_EQ(attachment.x, 0.0f);
    EXPECT_FLOAT_EQ(attachment.y, 0.0f);
    EXPECT_FLOAT_EQ(attachment.z, 1.0f);

    // Explicit attachment
    dot.SetTrailOffset(glm::vec3(0, 0, 5.0f));
    attachment = dot.GetTrailAttachmentPoint();
    EXPECT_FLOAT_EQ(attachment.z, 5.0f);
}

TEST(OffsetTest, ArrowAttachment) {
    Arrow arrow(1); // length 1.0, default
    // Default attachment for arrow should be at local origin
    glm::vec3 attachment = arrow.GetTrailAttachmentPoint();
    EXPECT_FLOAT_EQ(attachment.z, 0.0f);
}
