#include "grass_manager.h"
#include "graphics.h"
#include "terrain_render_manager.h"
#include "terrain_generator_interface.h"
#include "logger.h"
#include "profiler.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
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
        if (dummy_vao_) glDeleteVertexArrays(1, &dummy_vao_);
    }

    void GrassManager::Initialize() {
        if (initialized_) return;

        placement_shader_ = std::make_unique<ComputeShader>("shaders/grass_placement.comp");
        grass_shader_ = std::make_shared<Shader>("shaders/grass.vert", "shaders/grass.frag", "shaders/grass.tcs", "shaders/grass.tes");

        if (!placement_shader_->isValid() || !grass_shader_->ID) {
            logger::ERROR("Failed to compile grass shaders");
            return;
        }

        // The FrustumData and Lighting UBOs in frustum.glsl and lighting.glsl
        // don't have explicit binding qualifiers, so we need to wire them up manually.
        // Without this, the compute shader's frustum culling reads garbage and rejects all blades.
        GLuint frustum_idx = glGetUniformBlockIndex(placement_shader_->ID, "FrustumData");
        if (frustum_idx != GL_INVALID_INDEX)
            glUniformBlockBinding(placement_shader_->ID, frustum_idx, Constants::UboBinding::FrustumData());

        GLuint lighting_idx = glGetUniformBlockIndex(grass_shader_->ID, "Lighting");
        if (lighting_idx != GL_INVALID_INDEX)
            glUniformBlockBinding(grass_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());

        GLuint shadows_idx = glGetUniformBlockIndex(grass_shader_->ID, "Shadows");
        if (shadows_idx != GL_INVALID_INDEX)
            glUniformBlockBinding(grass_shader_->ID, shadows_idx, Constants::UboBinding::Shadows());

        GLuint terrain_idx = glGetUniformBlockIndex(grass_shader_->ID, "TerrainData");
        if (terrain_idx != GL_INVALID_INDEX)
            glUniformBlockBinding(grass_shader_->ID, terrain_idx, Constants::UboBinding::TerrainData());

        GLuint biome_idx = glGetUniformBlockIndex(grass_shader_->ID, "BiomeData");
        if (biome_idx != GL_INVALID_INDEX)
            glUniformBlockBinding(grass_shader_->ID, biome_idx, Constants::UboBinding::Biomes());

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
        DrawArraysIndirectCommand cmd = {1, 0, 0, 0}; // 1 vertex per blade (1-vertex patch)
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), &cmd, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        glGenVertexArrays(1, &dummy_vao_);
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

    void GrassManager::Render(const glm::mat4& view, const glm::mat4& projection, std::shared_ptr<TerrainRenderManager> renderManager, const RenderResources& res, bool isShadowPass) {
        if (!enabled_ || !initialized_) return;

        PROJECT_PROFILE_SCOPE("GrassManager::Render");

        grass_shader_->use();
        grass_shader_->setMat4("view", view);
        grass_shader_->setMat4("projection", projection);
        grass_shader_->setFloat("time", (float)glfwGetTime());
        grass_shader_->setBool("uIsShadowPass", isShadowPass);
        grass_shader_->setVec3("uCameraPos", last_camera_pos_);
        grass_shader_->setFloat("worldScale", 1.0f); // Fallback if needed, but usually bound via UBO

        if (renderManager) {
            renderManager->BindTerrainData(*grass_shader_);
        }

        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::GrassProps(), grass_props_ubo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassInstances(), grass_instances_ssbo_);
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), res.lightingUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(), res.shadowUbo);

        if (!isShadowPass) {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D_ARRAY, res.shadowMaps);
            grass_shader_->setInt("shadowMaps", 4);

            glActiveTexture(GL_TEXTURE20);
            glBindTexture(GL_TEXTURE_2D, res.transmittanceLUT);
            grass_shader_->setInt("u_transmittanceLUT", 20);

            glActiveTexture(GL_TEXTURE22);
            glBindTexture(GL_TEXTURE_2D, res.skyViewLUT);
            grass_shader_->setInt("u_skyViewLUT", 22);

            glActiveTexture(GL_TEXTURE23);
            glBindTexture(GL_TEXTURE_3D, res.aerialPerspectiveLUT);
            grass_shader_->setInt("u_aerialPerspectiveLUT", 23);

            if (res.cloudShadowMap) {
                glActiveTexture(GL_TEXTURE24);
                glBindTexture(GL_TEXTURE_2D, res.cloudShadowMap);
                grass_shader_->setInt("u_cloudShadowMap", 24);
            }

            grass_shader_->setFloat("u_atmosphereHeight", res.atmosphereHeight);

            // Noise textures are needed by the cloud shadow fallback path
            // (evaluateCloudShadowDensityAtWorldPos in clouds.glsl uses fastWorley3d)
            if (res.noiseTexture) {
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_3D, res.noiseTexture);
                grass_shader_->setInt("u_noiseTexture", 5);
            }
            if (res.curlTexture) {
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_3D, res.curlTexture);
                grass_shader_->setInt("u_curlTexture", 6);
            }
            if (res.extraNoiseTexture) {
                glActiveTexture(GL_TEXTURE8);
                glBindTexture(GL_TEXTURE_3D, res.extraNoiseTexture);
                grass_shader_->setInt("u_extraNoiseTexture", 8);
            }

            if (res.shadowIndices) {
                grass_shader_->setIntArray("lightShadowIndices", res.shadowIndices, 10);
            }
        }

        glBindVertexArray(dummy_vao_);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer_);

        glPatchParameteri(GL_PATCH_VERTICES, 1);

        glDisable(GL_CULL_FACE);
        glDrawArraysIndirect(GL_PATCHES, (void*)0);
        glEnable(GL_CULL_FACE);
        glBindVertexArray(0);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

}
