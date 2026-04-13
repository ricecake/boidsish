#include "grass_manager.h"
#include "graphics.h"
#include "terrain_render_manager.h"
#include "terrain_generator_interface.h"
#include "logger.h"
#include "profiler.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

    GrassManager::GrassManager() {
        for (int i = 0; i < 8; ++i) {
            biome_grass_props_[i].enabled = 0;
        }
    }

    GrassManager::~GrassManager() {
        if (grass_props_ubo_) glDeleteBuffers(1, &grass_props_ubo_);
        if (grass_instances_ssbo_) glDeleteBuffers(1, &grass_instances_ssbo_);
        if (grass_indirect_buffer_) glDeleteBuffers(1, &grass_indirect_buffer_);
    }

    void GrassManager::Initialize() {
        if (initialized_) return;

        placement_shader_ = std::make_unique<ComputeShader>("shaders/grass_placement.comp");
        grass_shader_ = std::make_shared<Shader>("shaders/grass.vert", "shaders/grass.frag", "shaders/grass.tcs", "shaders/grass.tes");

        if (!placement_shader_->isValid() || !grass_shader_->ID) {
            logger::ERROR("Failed to compile grass shaders");
            return;
        }

        // Set uniform block bindings for the grass shader
        GLuint lighting_idx = glGetUniformBlockIndex(grass_shader_->ID, "Lighting");
        if (lighting_idx != GL_INVALID_INDEX) {
            glUniformBlockBinding(grass_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
        }

        _InitializeResources();
        initialized_ = true;
        props_dirty_ = true;
    }

    void GrassManager::_InitializeResources() {
        // Properties UBO
        glGenBuffers(1, &grass_props_ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, grass_props_ubo_);
        glBufferData(GL_UNIFORM_BUFFER, 8 * sizeof(GrassProperties), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // Instance SSBO
        glGenBuffers(1, &grass_instances_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, grass_instances_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxGrassInstances * sizeof(GrassInstance), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Indirect Buffer
        glGenBuffers(1, &grass_indirect_buffer_);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer_);
        DrawArraysIndirectCommand cmd = {1, 0, 0, 0};
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), &cmd, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

    void GrassManager::SetGrassProperties(Biome biome, const GrassProperties& props) {
        int idx = static_cast<int>(biome);
        if (idx < 0 || idx >= 8) return;

        biome_grass_props_[idx] = props;
        biome_grass_props_[idx].enabled = 1;
        props_dirty_ = true;
    }

    void GrassManager::Update(float deltaTime, float time, const Camera& camera, const ITerrainGenerator& terrainGen, std::shared_ptr<TerrainRenderManager> renderManager) {
        if (!enabled_ || !initialized_) return;

        if (props_dirty_) {
            glBindBuffer(GL_UNIFORM_BUFFER, grass_props_ubo_);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, 8 * sizeof(GrassProperties), biome_grass_props_.data());
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            props_dirty_ = false;
        }

        PROJECT_PROFILE_SCOPE("GrassManager::Update");
        _UpdatePlacement(camera, terrainGen, renderManager);
    }

    void GrassManager::_UpdatePlacement(const Camera& camera, const ITerrainGenerator& terrainGen, std::shared_ptr<TerrainRenderManager> renderManager) {
        placement_shader_->use();

        renderManager->BindTerrainData(*placement_shader_);

        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::GrassProps(), grass_props_ubo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassInstances(), grass_instances_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassIndirect(), grass_indirect_buffer_);

        placement_shader_->setVec3("uCameraPos", camera.pos());
        placement_shader_->setFloat("uWorldScale", terrainGen.GetWorldScale());
        placement_shader_->setFloat("uMaxInstances", (float)kMaxGrassInstances);

        uint32_t zero = 0;
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer_);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, offsetof(DrawArraysIndirectCommand, instanceCount), sizeof(uint32_t), &zero);

        glDispatchCompute(128, 128, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    }

    void GrassManager::Render(const glm::mat4& view, const glm::mat4& projection, uint32_t lightingUbo, bool isShadowPass) {
        if (!enabled_ || !initialized_) return;

        PROJECT_PROFILE_SCOPE("GrassManager::Render");

        grass_shader_->use();
        grass_shader_->setMat4("view", view);
        grass_shader_->setMat4("projection", projection);
        grass_shader_->setBool("uIsShadowPass", isShadowPass);
        grass_shader_->setVec3("uCameraPos", last_camera_pos_);

        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::GrassProps(), grass_props_ubo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassInstances(), grass_instances_ssbo_);
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), lightingUbo);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer_);

        glPatchParameteri(GL_PATCH_VERTICES, 1);

        glDisable(GL_CULL_FACE);
        glDrawArraysIndirect(GL_PATCHES, 0);
        glEnable(GL_CULL_FACE);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

}
