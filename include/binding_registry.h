#pragma once

#include <array>
#include <cstddef>

#include "constants.h"

namespace Boidsish::BindingValidation {

template <size_t N>
consteval bool AllUnique(const std::array<int, N>& values) {
	for (size_t i = 0; i < N; ++i) {
		for (size_t j = i + 1; j < N; ++j) {
			if (values[i] == values[j]) {
				throw "Binding point conflict detected";
			}
		}
	}
	return true;
}

consteval bool ValidateUboBindings() {
	constexpr std::array ubos = {
		Constants::UboBinding::Lighting(),
		Constants::UboBinding::VisualEffects(),
		Constants::UboBinding::Shadows(),
		Constants::UboBinding::FrustumData(),
		Constants::UboBinding::Shockwaves(),
		Constants::UboBinding::TemporalData(),
		Constants::UboBinding::Biomes(),
		Constants::UboBinding::TerrainData(),
		Constants::UboBinding::GrassProps(),
		Constants::UboBinding::DecorProps(),
		Constants::UboBinding::DecorPlacementGlobals(),
		Constants::UboBinding::WeatherUniforms(),
		Constants::UboBinding::WindData(),
		Constants::UboBinding::VolumetricLighting(),
	};
	return AllUnique(ubos);
}

consteval bool ValidateSsboBindings() {
	constexpr std::array ssbos = {
		Constants::SsboBinding::DecorInstances(),
		Constants::SsboBinding::AutoExposure(),
		Constants::SsboBinding::BoneMatrix(),
		Constants::SsboBinding::OcclusionVisibility(),
		Constants::SsboBinding::ParticleGridHeads(),
		Constants::SsboBinding::ParticleGridNext(),
		Constants::SsboBinding::ParticleBuffer(),
		Constants::SsboBinding::IndirectionBuffer(),
		Constants::SsboBinding::TrailPoints(),
		Constants::SsboBinding::TrailInstances(),
		Constants::SsboBinding::TrailSpineData(),
		Constants::SsboBinding::CommonUniforms(),
		Constants::SsboBinding::EmitterBuffer(),
		Constants::SsboBinding::TerrainChunkInfo(),
		Constants::SsboBinding::SliceData(),
		Constants::SsboBinding::DecorAllInstances(),
		Constants::SsboBinding::DecorChunkParams(),
		Constants::SsboBinding::VisibleParticleIndices(),
		Constants::SsboBinding::ParticleDrawCommand(),
		Constants::SsboBinding::TerrainProbes(),
		Constants::SsboBinding::LiveParticleIndices(),
		Constants::SsboBinding::BehaviorDrawCommand(),
		Constants::SsboBinding::GrassInstances(),
		Constants::SsboBinding::GrassIndirect(),
		Constants::SsboBinding::WeatherGridA(),
		Constants::SsboBinding::WeatherGridB(),
		Constants::SsboBinding::DecorIndirect(),
		Constants::SsboBinding::DecorBlockValidity(),
		Constants::SsboBinding::MeshExplosionFragments(),
		Constants::SsboBinding::TrailGeneratedVBO(),
		Constants::SsboBinding::AtmosphereSH(),
		Constants::SsboBinding::SdfVolumes(),
	};
	return AllUnique(ssbos);
}

static_assert(ValidateUboBindings(), "UBO binding point conflict detected");
static_assert(ValidateSsboBindings(), "SSBO binding point conflict detected");

} // namespace Boidsish::BindingValidation
