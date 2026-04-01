#include <gtest/gtest.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "opengl_buffer.h"
#include <stdexcept>
#include <iostream>

namespace Boidsish {


TEST(OpenGLBufferTest, DirtyTrackingNoGL) {
    // Test dirty tracking without requiring an OpenGL context
    // We use a dummy target that won't be used for real GL calls here
    TypedBuffer<float> buffer(GL_ARRAY_BUFFER);
    std::vector<float> data = {1.0f, 2.0f, 3.0f};

    EXPECT_FALSE(buffer.IsDirty());
    buffer.SetData(data);
    EXPECT_TRUE(buffer.IsDirty());
    EXPECT_EQ(buffer.Size(), 3u);

    // Sync() would fail without GL context, so we don't call it here
    // or we could mock glBufferData if we really wanted to.

    buffer.GetData().push_back(4.0f);
    EXPECT_TRUE(buffer.IsDirty());
    EXPECT_EQ(buffer.Size(), 4u);
}

TEST(OpenGLBufferTest, BufferTypeCheck) {
    VertexBuffer<float> vbo;
    EXPECT_EQ(vbo.GetTarget(), (GLenum)GL_ARRAY_BUFFER);

    IndexBuffer<uint32_t> ebo;
    EXPECT_EQ(ebo.GetTarget(), (GLenum)GL_ELEMENT_ARRAY_BUFFER);

    UniformBuffer<float> ubo;
    EXPECT_EQ(ubo.GetTarget(), (GLenum)GL_UNIFORM_BUFFER);

    ShaderStorageBuffer<float> ssbo;
    EXPECT_EQ(ssbo.GetTarget(), (GLenum)GL_SHADER_STORAGE_BUFFER);

    IndirectBuffer<uint32_t> ibo;
    EXPECT_EQ(ibo.GetTarget(), (GLenum)GL_DRAW_INDIRECT_BUFFER);
}

} // namespace Boidsish

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    try {
        return RUN_ALL_TESTS();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
