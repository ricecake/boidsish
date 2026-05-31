#include "grass_manager.h"

#include "service_locator.h"
#include "state.h"
#include "graphics.h"
#include "terrain_render_manager.h"
#include "terrain_generator_interface.h"
#include "logger.h"
#include "profiler.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

    GrassManager::GrassManager(ServiceLocator& /*loc*/) {
        for (int i = 0; i < 8; ++i) {
            biome_grass_props_[i].enabled = 0;
        }
        global_props_ = GlobalGrassProperties();
    }

    GrassManager::~GrassManager() {
        if (grass_props_ubo_) glDeleteBuffers(1, &grass_props_ubo_);
        if (grass_instances_ssbo_) glDeleteBuffers(1, &grass_instances_ssbo_);
        if (grass_indirect_buffer_) glDeleteBuffers(1, &grass_indirect_buffer_);
        if (grass_tasks_ssbo_) glDeleteBuffers(1, &grass_tasks_ssbo_);
        if (dummy_vao_) glDeleteVertexArrays(1, &dummy_vao_);
    }

    void GrassManager::Initialize() {
        if (initialized_) return;

        PopulateDefaultGrassProperties();

        placement_shader_ = std::make_unique<ComputeShader>("shaders/grass_placement.comp");
        pre_pass_shader_ = std::make_unique<ComputeShader>("shaders/grass_pre_pass.comp");
        fixup_shader_ = std::make_unique<ComputeShader>("shaders/grass_command_fixup.comp");
        grass_shader_ = std::make_shared<Shader>("shaders/grass.vert", "shaders/grass.frag", "shaders/grass.tcs", "shaders/grass.tes");

        if (!placement_shader_->isValid() || !pre_pass_shader_->isValid() || !fixup_shader_->isValid() || !grass_shader_->ID) {
            logger::ERROR("Failed to compile grass shaders");
            return;
        }

        // The FrustumData and Lighting UBOs in frustum.glsl and lighting.glsl
        // don't have explicit binding qualifiers, so we need to wire them up manually.
        // Without this, the compute shader's frustum culling reads garbage and rejects all blades.
        placement_shader_->bindUniformBlock("FrustumData", Constants::UboBinding::FrustumData());
        pre_pass_shader_->bindUniformBlock("FrustumData", Constants::UboBinding::FrustumData());

        grass_shader_->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
        grass_shader_->bindUniformBlock("Shadows", Constants::UboBinding::Shadows());
        grass_shader_->bindUniformBlock("TerrainData", Constants::UboBinding::TerrainData());
        grass_shader_->bindUniformBlock("BiomeData", Constants::UboBinding::Biomes());

        _InitializeResources();
        initialized_ = true;
        props_dirty_ = true;
    }

    void GrassManager::_InitializeResources() {
        // Properties UBO
        glGenBuffers(1, &grass_props_ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, grass_props_ubo_);
        glBufferData(GL_UNIFORM_BUFFER, 8 * sizeof(GrassProperties) + sizeof(GlobalGrassProperties), nullptr, GL_DYNAMIC_DRAW);
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

        // Task SSBO
        glGenBuffers(1, &grass_tasks_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, grass_tasks_ssbo_);
        // Header (uint num_groups_x, y, z, taskCount) + tasks
        size_t tasks_size = 16 + kMaxGrassTasks * sizeof(GrassTask);
        glBufferData(GL_SHADER_STORAGE_BUFFER, tasks_size, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glGenVertexArrays(1, &dummy_vao_);
    }

    void GrassManager::SetGrassProperties(Biome biome, const GrassProperties& props) {
        int idx = static_cast<int>(biome);
        if (idx < 0 || idx >= 8) return;

        biome_grass_props_[idx] = props;
        biome_grass_props_[idx].enabled = 1;
        props_dirty_ = true;
    }

    void GrassManager::PopulateDefaultGrassProperties() {
        // Lush Grass
        GrassProperties lushGrass;
        lushGrass.colorTop = glm::vec4(0.3f, 0.8f, 0.2f, 1.0f);
        lushGrass.colorBottom = glm::vec4(0.1f, 0.3f, 0.05f, 1.0f);
        lushGrass.height = 1.0f;
        lushGrass.width = 0.1f;
        lushGrass.density = 0.8f;
        lushGrass.windInfluence = 1.0f;
        lushGrass.rigidity = 0.3f;
        lushGrass.flowerRatio = 0.05f;
        SetGrassProperties(Biome::LushGrass, lushGrass);

        // Dry Grass
        GrassProperties dryGrass;
        dryGrass.colorTop = glm::vec4(0.7f, 0.6f, 0.3f, 1.0f);
        dryGrass.colorBottom = glm::vec4(0.3f, 0.25f, 0.1f, 1.0f);
        dryGrass.height = 0.8f;
        dryGrass.width = 0.08f;
        dryGrass.density = 0.6f;
        dryGrass.windInfluence = 0.6f;
        dryGrass.rigidity = 0.6f;
        SetGrassProperties(Biome::DryGrass, dryGrass);

        // Forest Grass
        GrassProperties forestGrass;
        forestGrass.colorTop = glm::vec4(0.1f, 0.4f, 0.1f, 1.0f);
        forestGrass.colorBottom = glm::vec4(0.02f, 0.1f, 0.02f, 1.0f);
        forestGrass.height = 1.5f;
        forestGrass.width = 0.12f;
        forestGrass.density = 0.75f;
        forestGrass.windInfluence = 0.4f;
        forestGrass.rigidity = 0.5f;
        SetGrassProperties(Biome::Forest, forestGrass);

        // Alpine Meadow Grass
        GrassProperties alpineGrass;
        alpineGrass.colorTop = glm::vec4(0.4f, 0.9f, 0.4f, 1.0f);
        alpineGrass.colorBottom = glm::vec4(0.1f, 0.4f, 0.1f, 1.0f);
        alpineGrass.height = 0.6f;
        alpineGrass.width = 0.06f;
        alpineGrass.density = 0.7f;
        alpineGrass.windInfluence = 1.2f;
        alpineGrass.rigidity = 0.2f;
        alpineGrass.flowerRatio = 0.15f;
        SetGrassProperties(Biome::AlpineMeadow, alpineGrass);

        // Add some basic grass properties to other biomes to ensure we always have some coverage
        GrassProperties rockGrass;
        rockGrass.colorTop = glm::vec4(0.35f, 0.4f, 0.2f, 1.0f);
        rockGrass.colorBottom = glm::vec4(0.1f, 0.15f, 0.05f, 1.0f);
        rockGrass.height = 0.4f;
        rockGrass.width = 0.05f;
        rockGrass.density = 0.3f;
        rockGrass.windInfluence = 0.4f;
        rockGrass.rigidity = 0.7f;
        SetGrassProperties(Biome::BrownRock, rockGrass);
        SetGrassProperties(Biome::GreyRock, rockGrass);

        GrassProperties sandGrass;
        sandGrass.colorTop = glm::vec4(0.5f, 0.5f, 0.2f, 1.0f);
        sandGrass.colorBottom = glm::vec4(0.2f, 0.2f, 0.05f, 1.0f);
        sandGrass.height = 0.3f;
        sandGrass.width = 0.04f;
        sandGrass.density = 0.2f;
        sandGrass.windInfluence = 0.5f;
        sandGrass.rigidity = 0.4f;
        SetGrassProperties(Biome::Sand, sandGrass);

        GrassProperties snowGrass;
        snowGrass.colorTop = glm::vec4(0.8f, 0.9f, 0.95f, 1.0f);
        snowGrass.colorBottom = glm::vec4(0.4f, 0.5f, 0.55f, 1.0f);
        snowGrass.height = 0.2f;
        snowGrass.width = 0.04f;
        snowGrass.density = 0.1f;
        snowGrass.windInfluence = 0.2f;
        snowGrass.rigidity = 0.8f;
        SetGrassProperties(Biome::Snow, snowGrass);
    }

    void GrassManager::Update(float deltaTime, float time, const Camera& camera, const ITerrainGenerator& terrainGen, std::shared_ptr<TerrainRenderManager> renderManager) {
        if (!initialized_) return;

        last_camera_pos_ = camera.pos();

        if (props_dirty_) {
            glBindBuffer(GL_UNIFORM_BUFFER, grass_props_ubo_);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, 8 * sizeof(GrassProperties), biome_grass_props_.data());
            glBufferSubData(GL_UNIFORM_BUFFER, 8 * sizeof(GrassProperties), sizeof(GlobalGrassProperties), &global_props_);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            props_dirty_ = false;
        }

        if (!IsEnabled()) return;

        PROJECT_PROFILE_SCOPE("GrassManager::Update");
        _UpdatePlacement(camera, terrainGen, renderManager);
    }

    void GrassManager::_UpdatePlacement(const Camera& camera, const ITerrainGenerator& terrainGen, std::shared_ptr<TerrainRenderManager> renderManager) {
        // 1. Pre-pass: Build task queue
        pre_pass_shader_->use();
        renderManager->BindTerrainData(*pre_pass_shader_);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainPatchVisibility(), renderManager->GetPatchVisibilitySSBO());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainPatchMetrics(), renderManager->GetPatchMetricsSSBO());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassTasks(), grass_tasks_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::IndirectionBuffer(), renderManager->GetInstanceBuffer());

        pre_pass_shader_->setVec3("uCameraPos", camera.pos());
        pre_pass_shader_->setFloat("uWorldScale", terrainGen.GetWorldScale());
        pre_pass_shader_->setInt("u_numChunks", (int)renderManager->GetVisibleChunkCount());

        uint32_t zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, grass_tasks_ssbo_);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 12, sizeof(uint32_t), &zero); // zero out taskCount

        int numPatchesPerChunk = Constants::Class::Terrain::PatchesPerChunk();
        int total_patches = (int)renderManager->GetVisibleChunkCount() * numPatchesPerChunk;
        if (total_patches > 0) {
            glDispatchCompute((total_patches + 63) / 64, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        // Fixup: Set dispatch counts
        fixup_shader_->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassTasks(), grass_tasks_ssbo_);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

        // 2. Placement: Process tasks
        placement_shader_->use();

        renderManager->BindTerrainData(*placement_shader_);

        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::GrassProps(), grass_props_ubo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassInstances(), grass_instances_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassIndirect(), grass_indirect_buffer_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GrassTasks(), grass_tasks_ssbo_);

        placement_shader_->setVec3("uCameraPos", camera.pos());
        placement_shader_->setFloat("uWorldScale", terrainGen.GetWorldScale());
        placement_shader_->setFloat("uMaxInstances", (float)kMaxGrassInstances);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer_);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, offsetof(DrawArraysIndirectCommand, instanceCount), sizeof(uint32_t), &zero);

        // Dispatch placement based on taskCount
        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, grass_tasks_ssbo_);
        glDispatchComputeIndirect(0);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    }

    void GrassManager::Render(const glm::mat4& view, const glm::mat4& projection, std::shared_ptr<TerrainRenderManager> renderManager, const RenderResources& res, bool isShadowPass) {
        if (!IsEnabled() || !initialized_) return;

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
        if (res.lightingUboSize > 0) {
            glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(),
                res.lightingUbo, res.lightingUboOffset, res.lightingUboSize);
        } else {
            glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), res.lightingUbo);
        }
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(), res.shadowUbo);

        if (!isShadowPass) {
            glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::ShadowMaps());
            glBindTexture(GL_TEXTURE_2D_ARRAY, res.shadowMaps);
            grass_shader_->setInt("shadowMaps", Constants::TextureUnit::ShadowMaps());

            glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereTransmittance());
            glBindTexture(GL_TEXTURE_2D, res.transmittanceLUT);
            grass_shader_->setInt("u_transmittanceLUT", Constants::TextureUnit::AtmosphereTransmittance());

            glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereSkyView());
            glBindTexture(GL_TEXTURE_2D, res.skyViewLUT);
            grass_shader_->setInt("u_skyViewLUT", Constants::TextureUnit::AtmosphereSkyView());

            glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereAerialPerspective());
            glBindTexture(GL_TEXTURE_3D, res.aerialPerspectiveLUT);
            grass_shader_->setInt("u_aerialPerspectiveLUT", Constants::TextureUnit::AtmosphereAerialPerspective());

            if (res.cloudShadowMap) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereCloudShadow());
                glBindTexture(GL_TEXTURE_2D, res.cloudShadowMap);
                grass_shader_->setInt("u_cloudShadowMap", Constants::TextureUnit::AtmosphereCloudShadow());
            }

            grass_shader_->setFloat("u_atmosphereHeight", res.atmosphereHeight);

            // Noise textures are needed by the cloud shadow fallback path
            // (evaluateCloudShadowDensityAtWorldPos in clouds.glsl uses fastWorley3d)
            if (res.noiseTexture) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseSimplex());
                glBindTexture(GL_TEXTURE_3D, res.noiseTexture);
                grass_shader_->setInt("u_noiseTexture", Constants::TextureUnit::NoiseSimplex());
            }
            if (res.curlTexture) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseCurl());
                glBindTexture(GL_TEXTURE_3D, res.curlTexture);
                grass_shader_->setInt("u_curlTexture", Constants::TextureUnit::NoiseCurl());
            }
            if (res.extraNoiseTexture) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseExtra());
                glBindTexture(GL_TEXTURE_3D, res.extraNoiseTexture);
                grass_shader_->setInt("u_extraNoiseTexture", Constants::TextureUnit::NoiseExtra());
            }
            if (res.blueNoiseTexture) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseBlue());
                glBindTexture(GL_TEXTURE_2D, res.blueNoiseTexture);
                grass_shader_->trySetInt("u_blueNoiseTexture", Constants::TextureUnit::NoiseBlue());
            }
            if (res.phasorTexture) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoisePhasor());
                glBindTexture(GL_TEXTURE_2D, res.phasorTexture);
                grass_shader_->trySetInt("u_phasorTexture", Constants::TextureUnit::NoisePhasor());
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

    void GrassManager::SyncState() {
        if (!initialized_) return;
        auto store = ServiceLocator::Instance().Get<state::Store>();
        state::GrassSettings actual;
        actual.enabled = IsEnabled();
        actual.lengthMultiplier = global_props_.lengthMultiplier;
        actual.widthMultiplier = global_props_.widthMultiplier;
        actual.densityMultiplier = global_props_.densityMultiplier;
        actual.rigidityMultiplier = global_props_.rigidityMultiplier;
        actual.windMultiplier = global_props_.windMultiplier;
        store->Dispatch(state::actions::SyncGrassActual{actual});
    }

    void GrassManager::ApplyTargetState(const state::GrassSettings& config) {
        SetEnabled(config.enabled);
        GlobalGrassProperties props = GetGlobalProperties();
        props.lengthMultiplier = config.lengthMultiplier;
        props.widthMultiplier = config.widthMultiplier;
        props.densityMultiplier = config.densityMultiplier;
        props.rigidityMultiplier = config.rigidityMultiplier;
        props.windMultiplier = config.windMultiplier;
        SetGlobalProperties(props);
    }

}
