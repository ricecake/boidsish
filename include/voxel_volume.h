#pragma once

#include "shape.h"
#include <memory>

namespace Boidsish {

    class VoxelVolume : public Shape {
    public:
        VoxelVolume(float x = 0.0f, float y = 0.0f, float z = 0.0f, float size = 50.0f);

        void render() const override;
        void render(Shader& shader, const glm::mat4& model_matrix) const override;
        glm::mat4 GetModelMatrix() const override;

        void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;
        void PrepareResources(Megabuffer* megabuffer = nullptr) const override;

        std::string GetInstanceKey() const override { return "VoxelVolume"; }
        bool IsTransparent() const override { return true; }
        bool GetDefaultCastsShadows() const override { return false; }
        float GetBoundingRadius() const override { return size_ * 2.0f; }

        static void InitializeShaders();
        static ShaderHandle voxel_shader_handle;
        static std::shared_ptr<Shader> voxel_shader;

    private:
        float size_;
    };

} // namespace Boidsish
