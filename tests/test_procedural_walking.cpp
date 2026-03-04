#include <gtest/gtest.h>
#include "procedural_walking_creature.h"
#include "model.h"

using namespace Boidsish;

TEST(ProceduralWalkingCreatureTest, Initialization) {
    ProceduralWalkingCreature creature(0, 0, 0, 0, 4.0f);

    // Check that it doesn't crash during update
    creature.Update(0.016f);

    // Verify it has some vertices/meshes
    std::vector<RenderPacket> packets;
    RenderContext context;
    creature.GenerateRenderPackets(packets, context);

    EXPECT_GT(packets.size(), 0);
    for (const auto& p : packets) {
        EXPECT_GT(p.index_count, 0);
        // Should have bone matrices if it's skinned
        EXPECT_EQ(p.uniforms.use_skinning, 1);
        EXPECT_FALSE(p.bone_matrices.empty());
    }
}

TEST(ProceduralWalkingCreatureTest, Movement) {
    ProceduralWalkingCreature creature(0, 0, 0, 0, 4.0f);
    creature.SetTarget(glm::vec3(10, 0, 10));

    glm::vec3 startPos = glm::vec3(creature.GetModelMatrix()[3]);

    // Update for a few frames
    for(int i=0; i<60; ++i) {
        creature.Update(0.016f);
    }

    glm::vec3 endPos = glm::vec3(creature.GetModelMatrix()[3]);

    // Should have moved towards target
    EXPECT_GT(glm::distance(startPos, endPos), 0.1f);
}
