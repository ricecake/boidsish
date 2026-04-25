#pragma once

#include "constants.h"

namespace Boidsish {

	/**
	 * Centralized registration for shader variable replacements.
	 * Called once to register all [[CONSTANT_NAME]] tokens that the
	 * shader preprocessor substitutes before compilation.
	 *
	 * @tparam T A class providing static RegisterConstant(const std::string&, value).
	 */
	template<typename T>
	void RegisterAllShaderConstants() {
		// UBO Bindings
		T::RegisterConstant("LIGHTING_BINDING", Constants::UboBinding::Lighting());
		T::RegisterConstant("VISUAL_EFFECTS_BINDING", Constants::UboBinding::VisualEffects());
		T::RegisterConstant("SHADOWS_BINDING", Constants::UboBinding::Shadows());
		T::RegisterConstant("FRUSTUM_BINDING", Constants::UboBinding::FrustumData());
		T::RegisterConstant("SHOCKWAVES_BINDING", Constants::UboBinding::Shockwaves());
		T::RegisterConstant("SDF_VOLUMES_BINDING", Constants::SsboBinding::SdfVolumes());
		T::RegisterConstant("TEMPORAL_DATA_BINDING", Constants::UboBinding::TemporalData());
		T::RegisterConstant("BIOME_DATA_BINDING", Constants::UboBinding::Biomes());
		T::RegisterConstant("TERRAIN_DATA_BINDING", Constants::UboBinding::TerrainData());
		T::RegisterConstant("GRASS_PROPS_BINDING", Constants::UboBinding::GrassProps());
		T::RegisterConstant("DECOR_PROPS_BINDING", Constants::UboBinding::DecorProps());
		T::RegisterConstant("DECOR_PLACEMENT_GLOBALS_BINDING", Constants::UboBinding::DecorPlacementGlobals());
		T::RegisterConstant("WEATHER_UNIFORMS_BINDING", Constants::UboBinding::WeatherUniforms());
		T::RegisterConstant("WIND_DATA_BINDING", Constants::UboBinding::WindData());
		T::RegisterConstant("VOLUMETRIC_LIGHTING_BINDING", Constants::UboBinding::VolumetricLighting());

		// SSBO Bindings
		T::RegisterConstant("DECOR_INSTANCES_BINDING", Constants::SsboBinding::DecorInstances());
		T::RegisterConstant("AUTO_EXPOSURE_BINDING", Constants::SsboBinding::AutoExposure());
		T::RegisterConstant("BONE_MATRIX_BINDING", Constants::SsboBinding::BoneMatrix());
		T::RegisterConstant("OCCLUSION_VISIBILITY_BINDING", Constants::SsboBinding::OcclusionVisibility());
		T::RegisterConstant("PARTICLE_GRID_HEADS_BINDING", Constants::SsboBinding::ParticleGridHeads());
		T::RegisterConstant("PARTICLE_GRID_NEXT_BINDING", Constants::SsboBinding::ParticleGridNext());
		T::RegisterConstant("PARTICLE_BUFFER_BINDING", Constants::SsboBinding::ParticleBuffer());
		T::RegisterConstant("INDIRECTION_BUFFER_BINDING", Constants::SsboBinding::IndirectionBuffer());
		T::RegisterConstant("TRAIL_POINTS_BINDING", Constants::SsboBinding::TrailPoints());
		T::RegisterConstant("TRAIL_INSTANCES_BINDING", Constants::SsboBinding::TrailInstances());
		T::RegisterConstant("TRAIL_SPINE_DATA_BINDING", Constants::SsboBinding::TrailSpineData());
		T::RegisterConstant("COMMON_UNIFORMS_BINDING", Constants::SsboBinding::CommonUniforms());
		T::RegisterConstant("EMITTER_BUFFER_BINDING", Constants::SsboBinding::EmitterBuffer());
		T::RegisterConstant("TERRAIN_CHUNK_INFO_BINDING", Constants::SsboBinding::TerrainChunkInfo());
		T::RegisterConstant("SLICE_DATA_BINDING", Constants::SsboBinding::SliceData());
		T::RegisterConstant("DECOR_ALL_INSTANCES_BINDING", Constants::SsboBinding::DecorAllInstances());
		T::RegisterConstant("DECOR_CHUNK_PARAMS_BINDING", Constants::SsboBinding::DecorChunkParams());
		T::RegisterConstant("VISIBLE_PARTICLE_INDICES_BINDING", Constants::SsboBinding::VisibleParticleIndices());
		T::RegisterConstant("PARTICLE_DRAW_COMMAND_BINDING", Constants::SsboBinding::ParticleDrawCommand());
		T::RegisterConstant("TERRAIN_PROBES_BINDING", Constants::SsboBinding::TerrainProbes());
		T::RegisterConstant("LIVE_PARTICLE_INDICES_BINDING", Constants::SsboBinding::LiveParticleIndices());
		T::RegisterConstant("BEHAVIOR_DRAW_COMMAND_BINDING", Constants::SsboBinding::BehaviorDrawCommand());
		T::RegisterConstant("GRASS_INSTANCES_BINDING", Constants::SsboBinding::GrassInstances());
		T::RegisterConstant("GRASS_INDIRECT_BINDING", Constants::SsboBinding::GrassIndirect());
		T::RegisterConstant("WEATHER_GRID_A_BINDING", Constants::SsboBinding::WeatherGridA());
		T::RegisterConstant("WEATHER_GRID_B_BINDING", Constants::SsboBinding::WeatherGridB());
		T::RegisterConstant("DECOR_INDIRECT_BINDING", Constants::SsboBinding::DecorIndirect());
		T::RegisterConstant("DECOR_BLOCK_VALIDITY_BINDING", Constants::SsboBinding::DecorBlockValidity());
		T::RegisterConstant("MESH_EXPLOSION_FRAGMENTS_BINDING", Constants::SsboBinding::MeshExplosionFragments());
		T::RegisterConstant("TRAIL_GENERATED_VBO_BINDING", Constants::SsboBinding::TrailGeneratedVBO());
		T::RegisterConstant("ATMOSPHERE_SH_BINDING", Constants::SsboBinding::AtmosphereSH());

		// Limits and Sizes
		T::RegisterConstant("MAX_LIGHTS", Constants::Class::Shadows::MaxLights());
		T::RegisterConstant("MAX_SHADOW_MAPS", Constants::Class::Shadows::MaxShadowMaps());
		T::RegisterConstant("MAX_CASCADES", Constants::Class::Shadows::MaxCascades());
		T::RegisterConstant("MAX_SHOCKWAVES", Constants::Class::Shockwaves::MaxShockwaves());
		T::RegisterConstant("CHUNK_SIZE", Constants::Class::Terrain::ChunkSize());
		T::RegisterConstant("CHUNK_SIZE_PLUS_1", Constants::Class::Terrain::ChunkSizePlus1());

		// Texture unit bindings
		T::RegisterConstant("PHASOR_TEXTURE_BINDING", Constants::TextureUnit::NoisePhasor());
		T::RegisterConstant("WIND_TEXTURE_BINDING", Constants::TextureUnit::WindData());

		T::RegisterConstant("ATMOSPHERE_TRANSMITTANCE_BINDING", Constants::TextureUnit::AtmosphereTransmittance());
		T::RegisterConstant("ATMOSPHERE_SKY_VIEW_BINDING", Constants::TextureUnit::AtmosphereSkyView());
		T::RegisterConstant("ATMOSPHERE_AERIAL_PERSPECTIVE_BINDING", Constants::TextureUnit::AtmosphereAerialPerspective());
		T::RegisterConstant("ATMOSPHERE_CLOUD_SHADOW_BINDING", Constants::TextureUnit::AtmosphereCloudShadow());

		T::RegisterConstant("VOLUMETRIC_CASCADE0_BINDING", Constants::TextureUnit::VolumetricCascades());
		T::RegisterConstant("VOLUMETRIC_CASCADE1_BINDING", Constants::TextureUnit::VolumetricCascades() + 1);
		T::RegisterConstant("WEATHER_SCALARS_BINDING", Constants::TextureUnit::WeatherScalars());

		T::RegisterConstant("RAW_HEIGHTMAP_BINDING", Constants::TextureUnit::TerrainRawHeightmap());
		T::RegisterConstant("BAKED_HEIGHTMAP_BINDING", Constants::TextureUnit::TerrainHeightmap());
		T::RegisterConstant("BAKED_PARAMS_BINDING", Constants::TextureUnit::TerrainBakedParams());

		T::RegisterConstant("BIOME_MAP_IMAGE_BINDING", Constants::TextureUnit::TerrainBiomeImage());
		T::RegisterConstant("BAKED_HEIGHTMAP_IMAGE_BINDING", Constants::TextureUnit::TerrainHeightmapImage());
		T::RegisterConstant("BAKED_PARAMS_IMAGE_BINDING", Constants::TextureUnit::TerrainBakedParamsImage());

		T::RegisterConstant("TERRAIN_HORIZON_MAP_BINDING", Constants::TextureUnit::TerrainHorizonMap());
		T::RegisterConstant("TERRAIN_SHADOW_MAP_BINDING", Constants::TextureUnit::TerrainShadowMap());
		T::RegisterConstant("TERRAIN_SHADOW_MAP_IMAGE_BINDING", Constants::TextureUnit::TerrainShadowMapImage());
	}

} // namespace Boidsish
