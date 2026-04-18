#ifndef SHADER_REGISTRATION_H
#define SHADER_REGISTRATION_H

#include "constants.h"

namespace Boidsish {

    /**
     * @brief Centralized registration for shader variable replacements.
     * This function should be called by any system that needs to register 
     * common shader constants (e.g., binding sites, limits, and sizes).
     * 
     * @tparam T A class or object that provides a static or instance method 
     *           RegisterConstant(const std::string&, value).
     */
    template<typename T>
    void RegisterAllShaderConstants() {
        // UBO Bindings
        T::RegisterConstant("LIGHTING_BINDING", Boidsish::Constants::UboBinding::Lighting());
        T::RegisterConstant("VISUAL_EFFECTS_BINDING", Boidsish::Constants::UboBinding::VisualEffects());
        T::RegisterConstant("SHADOWS_BINDING", Boidsish::Constants::UboBinding::Shadows());
        T::RegisterConstant("FRUSTUM_BINDING", Boidsish::Constants::UboBinding::FrustumData());
        T::RegisterConstant("SHOCKWAVES_BINDING", Boidsish::Constants::UboBinding::Shockwaves());
        T::RegisterConstant("SDF_VOLUMES_BINDING", Boidsish::Constants::UboBinding::SdfVolumes());
        T::RegisterConstant("TEMPORAL_DATA_BINDING", Boidsish::Constants::UboBinding::TemporalData());
        T::RegisterConstant("BIOME_DATA_BINDING", Boidsish::Constants::UboBinding::Biomes());
        T::RegisterConstant("TERRAIN_DATA_BINDING", Boidsish::Constants::UboBinding::TerrainData());
        T::RegisterConstant("GRASS_PROPS_BINDING", Boidsish::Constants::UboBinding::GrassProps());
        T::RegisterConstant("DECOR_PROPS_BINDING", Boidsish::Constants::UboBinding::DecorProps());
        T::RegisterConstant("DECOR_PLACEMENT_GLOBALS_BINDING", Boidsish::Constants::UboBinding::DecorPlacementGlobals());
        T::RegisterConstant("WEATHER_UNIFORMS_BINDING", Boidsish::Constants::UboBinding::WeatherUniforms());

        // SSBO Bindings
        T::RegisterConstant("BONE_MATRIX_BINDING", Boidsish::Constants::SsboBinding::BoneMatrix());
        T::RegisterConstant("OCCLUSION_VISIBILITY_BINDING", Boidsish::Constants::SsboBinding::OcclusionVisibility());
        T::RegisterConstant("TERRAIN_PROBES_BINDING", Boidsish::Constants::SsboBinding::TerrainProbes());
        T::RegisterConstant("GRASS_INSTANCES_BINDING", Boidsish::Constants::SsboBinding::GrassInstances());
        T::RegisterConstant("GRASS_INDIRECT_BINDING", Boidsish::Constants::SsboBinding::GrassIndirect());
        T::RegisterConstant("DECOR_INSTANCES_BINDING", 10); // Binding 10 used in vis.vert but not in constants.h // Wait, memory says 10, constants.h says BoneMatrix is 12, but memory says DecorInstances is 10. Let's check constants.h again.
        T::RegisterConstant("WEATHER_GRID_A_BINDING", Boidsish::Constants::SsboBinding::WeatherGridA());
        T::RegisterConstant("WEATHER_GRID_B_BINDING", Boidsish::Constants::SsboBinding::WeatherGridB());
        T::RegisterConstant("AUTO_EXPOSURE_BINDING", Boidsish::Constants::SsboBinding::AutoExposure());
        T::RegisterConstant("TRAIL_POINTS_BINDING", Boidsish::Constants::SsboBinding::TrailPoints());
        T::RegisterConstant("TRAIL_INSTANCES_BINDING", Boidsish::Constants::SsboBinding::TrailInstances());
        T::RegisterConstant("TRAIL_SPINE_DATA_BINDING", Boidsish::Constants::SsboBinding::TrailSpineData());
        T::RegisterConstant("PARTICLE_BUFFER_BINDING", Boidsish::Constants::SsboBinding::ParticleBuffer());
        T::RegisterConstant("EMITTER_BUFFER_BINDING", Boidsish::Constants::SsboBinding::EmitterBuffer());
        T::RegisterConstant("VISIBLE_PARTICLE_INDICES_BINDING", Boidsish::Constants::SsboBinding::VisibleParticleIndices());
        T::RegisterConstant("LIVE_PARTICLE_INDICES_BINDING", Boidsish::Constants::SsboBinding::LiveParticleIndices());
        T::RegisterConstant("PARTICLE_GRID_HEADS_BINDING", Boidsish::Constants::SsboBinding::ParticleGridHeads());
        T::RegisterConstant("PARTICLE_GRID_NEXT_BINDING", Boidsish::Constants::SsboBinding::ParticleGridNext());
        T::RegisterConstant("INDIRECTION_BUFFER_BINDING", Boidsish::Constants::SsboBinding::IndirectionBuffer());
        T::RegisterConstant("TERRAIN_CHUNK_INFO_BINDING", Boidsish::Constants::SsboBinding::TerrainChunkInfo());
        T::RegisterConstant("SLICE_DATA_BINDING", Boidsish::Constants::SsboBinding::SliceData());
        T::RegisterConstant("PARTICLE_DRAW_COMMAND_BINDING", Boidsish::Constants::SsboBinding::ParticleDrawCommand());
        T::RegisterConstant("BEHAVIOR_DRAW_COMMAND_BINDING", Boidsish::Constants::SsboBinding::BehaviorDrawCommand());
        T::RegisterConstant("DECOR_CHUNK_PARAMS_BINDING", Boidsish::Constants::SsboBinding::DecorChunkParams());
        T::RegisterConstant("COMMON_UNIFORMS_BINDING", 21);
        T::RegisterConstant("TRAIL_GENERATED_VBO_BINDING", 42);
        T::RegisterConstant("MESH_EXPLOSION_FRAGMENTS_BINDING", 41);
        T::RegisterConstant("DECOR_ALL_INSTANCES_BINDING", 25);
        T::RegisterConstant("DECOR_BLOCK_VALIDITY_BINDING", 40);
        T::RegisterConstant("DECOR_INDIRECT_BINDING", 39);

        // Limits and Sizes
        T::RegisterConstant("MAX_LIGHTS", Boidsish::Constants::Class::Shadows::MaxLights());
        T::RegisterConstant("MAX_SHADOW_MAPS", Boidsish::Constants::Class::Shadows::MaxShadowMaps());
        T::RegisterConstant("MAX_CASCADES", Boidsish::Constants::Class::Shadows::MaxCascades());
        T::RegisterConstant("MAX_SHOCKWAVES", Boidsish::Constants::Class::Shockwaves::MaxShockwaves());
        T::RegisterConstant("CHUNK_SIZE", Boidsish::Constants::Class::Terrain::ChunkSize());
        T::RegisterConstant("CHUNK_SIZE_PLUS_1", Boidsish::Constants::Class::Terrain::ChunkSizePlus1());
    }

} // namespace Boidsish

#endif
