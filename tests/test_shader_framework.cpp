#include <gtest/gtest.h>
#include "handle.h"
#include "render_shader.h"
#include "shader_table.h"
#include "shader.h"

using namespace Boidsish;

// A concrete implementation of RenderShader for testing
class TestShader : public RenderShader {
public:
    using RenderShader::RenderShader;

    const std::vector<Field>& GetRequiredFields() const override {
        static std::vector<Field> fields = {{"time"}, {"resolution"}};
        return fields;
    }
};

TEST(ShaderFrameworkTest, HandleTest) {
    Handle<int> h1(1);
    Handle<int> h2(1);
    Handle<int> h3(2);
    Handle<float> h4(1);

    EXPECT_TRUE(h1.IsValid());
    EXPECT_EQ(h1, h2);
    EXPECT_NE(h1, h3);
    EXPECT_LT(h1, h3);

    // This shouldn't compile if Handle was not type-safe,
    // but they are different types Handle<int, int> and Handle<float, float>
    // EXPECT_NE(h1, h4); // Static error
}

TEST(ShaderFrameworkTest, ShaderTableTest) {
    ShaderTable table;

    // We can't easily create a real Shader object without a GL context,
    // so we'll use nullptr for backing shader for this unit test.
    auto shader = std::make_unique<TestShader>(nullptr);
    auto handle = table.Register(std::move(shader));

    EXPECT_TRUE(handle.IsValid());
    EXPECT_NE(table.Get(handle), nullptr);

    auto retrieved = table.Get(handle);
    EXPECT_EQ(retrieved->GetRequiredFields().size(), 2);
    EXPECT_EQ(retrieved->GetRequiredFields()[0].name, "time");

    table.Unregister(handle);
    EXPECT_EQ(table.Get(handle), nullptr);
}

TEST(ShaderFrameworkTest, UniformQueueTest) {
    ShaderTable table;
    auto shader = std::make_unique<TestShader>(nullptr);
    auto handle = table.Register(std::move(shader));

    auto retrieved = table.Get(handle);
    retrieved->SetUniform("testInt", 42);
    retrieved->SetUniform("testFloat", 3.14f);

    // Flush should not crash even with nullptr backing shader because of the check
    retrieved->Flush();
}
