#include "plant_manager.h"
#include "graphics.h"
#include "terrain_render_manager.h"
#include "terrain_generator_interface.h"
#include "grass_manager.h"
#include "logger.h"
#include "profiler.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

    PlantManager::PlantManager() {}

    PlantManager::~PlantManager() {
        if (plant_props_ubo_) glDeleteBuffers(1, &plant_props_ubo_);
        if (plant_instances_ssbo_) glDeleteBuffers(1, &plant_instances_ssbo_);
        if (plant_indirect_buffer_) glDeleteBuffers(1, &plant_indirect_buffer_);
        if (plant_generated_vbo_) glDeleteBuffers(1, &plant_generated_vbo_);
        if (dummy_vao_) glDeleteVertexArrays(1, &dummy_vao_);
    }

    void PlantManager::Initialize() {
        if (initialized_) return;

        placement_shader_ = std::make_unique<ComputeShader>("shaders/plant_placement.comp");
        update_shader_ = std::make_unique<ComputeShader>("shaders/plant_update.comp");
        plant_shader_ = std::make_shared<Shader>("shaders/plant.vert", "shaders/plant.frag");

        if (!placement_shader_->isValid() || !update_shader_->isValid() || !plant_shader_->ID) {
            logger::ERROR("Failed to compile plant shaders");
            // return; // Continue for now, maybe they will be created later
        }

        // Bind UBOs for the plant shader (Lighting, Shadows, PlantProps)
        GLuint lighting_idx = glGetUniformBlockIndex(plant_shader_->ID, "Lighting");
        if (lighting_idx != GL_INVALID_INDEX)
            glUniformBlockBinding(plant_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());

        GLuint shadows_idx = glGetUniformBlockIndex(plant_shader_->ID, "Shadows");
        if (shadows_idx != GL_INVALID_INDEX)
            glUniformBlockBinding(plant_shader_->ID, shadows_idx, Constants::UboBinding::Shadows());

        GLuint plant_idx = glGetUniformBlockIndex(plant_shader_->ID, "PlantProps");
        if (plant_idx != GL_INVALID_INDEX)
            glUniformBlockBinding(plant_shader_->ID, plant_idx, Constants::UboBinding::PlantProps());

        _InitializeResources();
        initialized_ = true;
    }

    void PlantManager::_InitializeResources() {
        glGenBuffers(1, &plant_props_ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, plant_props_ubo_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(PlantProperties), &props_, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &plant_instances_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, plant_instances_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxPlantInstances * sizeof(PlantInstance), nullptr, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &plant_indirect_buffer_);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, plant_indirect_buffer_);
        DrawArraysIndirectCommand cmd = {0, 1, 0, 0};
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), &cmd, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &plant_generated_vbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, plant_generated_vbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxPlantVertices * sizeof(PlantVertex), nullptr, GL_DYNAMIC_DRAW);

        glGenVertexArrays(1, &dummy_vao_);
        glBindVertexArray(dummy_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, plant_generated_vbo_);
        // pos (xyz) + u (w)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(PlantVertex), (void*)offsetof(PlantVertex, pos));
        // normal (xyz) + v (w)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PlantVertex), (void*)offsetof(PlantVertex, normal));
        // color (rgb) + type (w)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(PlantVertex), (void*)offsetof(PlantVertex, color));
        // Also bind the indirect buffer to the VAO so it's ready for DrawArraysIndirect
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, plant_indirect_buffer_);
        glBindVertexArray(0);
    }

    void PlantManager::Update(float deltaTime, float time, const Camera& camera, const ITerrainGenerator& terrainGen, std::shared_ptr<TerrainRenderManager> renderManager, uint32_t grassInstancesSSBO, uint32_t grassIndirectBuffer) {
        if (!initialized_ || !enabled_) return;

        if (props_dirty_) {
            glBindBuffer(GL_UNIFORM_BUFFER, plant_props_ubo_);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlantProperties), &props_);
            props_dirty_ = false;
        }

        PROJECT_PROFILE_SCOPE("PlantManager::Update");

        // 1. Placement
        if (placement_shader_->isValid()) {
            placement_shader_->use();
            renderManager->BindTerrainData(*placement_shader_);
            glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::PlantProps(), plant_props_ubo_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::PlantInstances(), plant_instances_ssbo_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::PlantIndirect(), plant_indirect_buffer_);

            placement_shader_->setVec3("uCameraPos", camera.pos());
            placement_shader_->setFloat("uWorldScale", terrainGen.GetWorldScale());
            placement_shader_->setFloat("uMaxInstances", (float)kMaxPlantInstances);

            // The terrain data UBO/Textures are already bound by BindTerrainData

            // Reset instanceCount in indirect buffer for placement (we use instanceCount as a counter here)
            uint32_t zero = 0;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, plant_indirect_buffer_);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(DrawArraysIndirectCommand, instanceCount), sizeof(uint32_t), &zero);

            glDispatchCompute(64, 64, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        // 2. Generation (Update VBO and Grass)
        if (update_shader_->isValid()) {
            update_shader_->use();
            glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::PlantProps(), plant_props_ubo_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::PlantInstances(), plant_instances_ssbo_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::PlantIndirect(), plant_indirect_buffer_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::PlantGeneratedVBO(), plant_generated_vbo_);

            // Output to grass system
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassInstances(), grassInstancesSSBO);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassIndirect(), grassIndirectBuffer);

            update_shader_->setFloat("uTime", time);
            update_shader_->setFloat("uWorldScale", terrainGen.GetWorldScale());
            update_shader_->setFloat("uMaxVertices", (float)kMaxPlantVertices);

            // Reset vertex count in indirect buffer
            uint32_t zero = 0;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, plant_indirect_buffer_);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(DrawArraysIndirectCommand, count), sizeof(uint32_t), &zero);

            // Dispatch based on instances placed
            glDispatchCompute((kMaxPlantInstances + 63) / 64, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

            // CRITICAL: The indirect buffer's instanceCount must be 1 for the final draw call.
            // We used it as a counter for placement, but all geometry is generated into one large buffer.
            uint32_t one = 1;
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(DrawArraysIndirectCommand, instanceCount), sizeof(uint32_t), &one);
        }
    }

    void PlantManager::Render(const glm::mat4& view, const glm::mat4& projection, const GrassManager::RenderResources& res, uint32_t temporalUbo, bool isShadowPass) {
        if (!initialized_ || !enabled_) return;

        PROJECT_PROFILE_SCOPE("PlantManager::Render");

        plant_shader_->use();
        plant_shader_->setMat4("view", view);
        plant_shader_->setMat4("projection", projection);
        plant_shader_->setFloat("uTime", res.time);
        plant_shader_->setBool("uIsShadowPass", isShadowPass);

        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), res.lightingUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(), res.shadowUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::PlantProps(), plant_props_ubo_);
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TemporalData(), temporalUbo);

        if (!isShadowPass) {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D_ARRAY, res.shadowMaps);
            plant_shader_->setInt("shadowMaps", 4);
            // ... bind other textures if needed ...
        }

        glBindVertexArray(dummy_vao_);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, plant_indirect_buffer_);

        glDisable(GL_CULL_FACE);
        glDrawArraysIndirect(GL_TRIANGLES, (void*)0);
        glEnable(GL_CULL_FACE);

        glBindVertexArray(0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

}
