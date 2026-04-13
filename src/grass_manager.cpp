#include "grass_manager.h"
#include "graphics.h"
#include "terrain_render_manager.h"
#include "terrain_generator_interface.h"
#include "logger.h"
#include "profiler.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

    GrassManager::GrassManager() {}

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

        _InitializeResources();
        initialized_ = true;

        // Sync any types added before initialization
        if (!grass_types_.empty()) {
            glBindBuffer(GL_UNIFORM_BUFFER, grass_props_ubo_);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GrassProperties), &grass_types_[0].props);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }
    }

    void GrassManager::_InitializeResources() {
        // Properties UBO
        glGenBuffers(1, &grass_props_ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, grass_props_ubo_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(GrassProperties), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // Instance SSBO
        glGenBuffers(1, &grass_instances_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, grass_instances_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxGrassInstances * sizeof(GrassInstance), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Indirect Buffer
        glGenBuffers(1, &grass_indirect_buffer_);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer_);
        DrawArraysIndirectCommand cmd = {1, 0, 0, 0}; // 1 vertex per blade (the patch is the blade)
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), &cmd, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

    void GrassManager::AddGrassType(const std::string& name, const GrassProperties& props) {
        grass_types_.push_back({props, name});
        if (initialized_ && grass_props_ubo_) {
            glBindBuffer(GL_UNIFORM_BUFFER, grass_props_ubo_);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GrassProperties), &grass_types_[0].props);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }
    }

    void GrassManager::Update(float deltaTime, const Camera& camera, const ITerrainGenerator& terrainGen, std::shared_ptr<TerrainRenderManager> renderManager) {
        if (!enabled_ || !initialized_ || grass_types_.empty()) return;

        PROJECT_PROFILE_SCOPE("GrassManager::Update");
        _UpdatePlacement(camera, terrainGen, renderManager);
    }

    void GrassManager::_UpdatePlacement(const Camera& camera, const ITerrainGenerator& terrainGen, std::shared_ptr<TerrainRenderManager> renderManager) {
        placement_shader_->use();

        // Bind terrain data
        renderManager->BindTerrainData(*placement_shader_);

        // Bind buffers
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::GrassProps(), grass_props_ubo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassInstances(), grass_instances_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassIndirect(), grass_indirect_buffer_);

        // Set uniforms
        placement_shader_->setVec3("uCameraPos", camera.pos());
        placement_shader_->setFloat("uWorldScale", terrainGen.GetWorldScale());
        placement_shader_->setFloat("uMaxInstances", (float)kMaxGrassInstances);

        // Reset instance count in indirect buffer
        uint32_t zero = 0;
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer_);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, offsetof(DrawArraysIndirectCommand, instanceCount), sizeof(uint32_t), &zero);

        glDispatchCompute(128, 128, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    }

    void GrassManager::Render(const glm::mat4& view, const glm::mat4& projection, bool isShadowPass) {
        if (!enabled_ || !initialized_ || grass_types_.empty()) return;

        PROJECT_PROFILE_SCOPE("GrassManager::Render");

        grass_shader_->use();
        grass_shader_->setMat4("view", view);
        grass_shader_->setMat4("projection", projection);
        grass_shader_->setBool("uIsShadowPass", isShadowPass);
        grass_shader_->setVec3("uCameraPos", last_camera_pos_); // Needs to be updated or passed

        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::GrassProps(), grass_props_ubo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassInstances(), grass_instances_ssbo_);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer_);

        glPatchParameteri(GL_PATCH_VERTICES, 1);

        glDisable(GL_CULL_FACE);
        glDrawArraysIndirect(GL_PATCHES, 0);
        glEnable(GL_CULL_FACE);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

}
