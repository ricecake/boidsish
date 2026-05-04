#pragma once
#include <array>
#include <bitset>
#include <initializer_list>

#include <glm/glm.hpp>

namespace Boidsish {

	enum class Biome : uint32_t {
		Sand = 0,
		LushGrass,
		DryGrass,
		Forest,
		AlpineMeadow,
		BrownRock,
		GreyRock,
		Snow,
		Count
	};

	struct BiomeBitset {
		std::bitset<static_cast<size_t>(Biome::Count)> bits;

		BiomeBitset() { bits.set(); }

		BiomeBitset(uint32_t mask): bits(mask) {}

		BiomeBitset(std::initializer_list<Biome> biomes) {
			bits.reset();
			for (auto b : biomes) {
				bits.set(static_cast<size_t>(b));
			}
		}

		void set(Biome b, bool val = true) { bits.set(static_cast<size_t>(b), val); }

		void reset() { bits.reset(); }

		bool test(Biome b) const { return bits.test(static_cast<size_t>(b)); }

		operator uint32_t() const { return static_cast<uint32_t>(bits.to_ulong()); }
	};

	struct BiomeAttributes {
		float     spikeDamping;  // How aggressively to cut off sharp gradients
		float     detailMasking; // How much valleys should hide high-frequency noise
		float     floorLevel;    // The height at which flattening occurs
		float     weight = 1.0f; // How much weight to give this Biome
		glm::vec3 albedo;
		float     roughness;
		float     metallic;
		float     detailStrength;
		float     detailScale;
		float     noiseType = 1.0f; // 0: Simplex, 1: Worley, 2: FBM, 3: Ridge

		// Weather System Properties
		float     dragFactor;         // Contribution to LBM fluid drag (0.0 - 1.0)
		float     aerosolReleaseRate; // Rate of aerosol injection into the LBM (dust/pollen)
		float     sensibleHeatFactor; // Efficiency of converting solar radiation to air temperature
		glm::vec3 aerosolColor;       // Visual color of the released aerosol
	};

	struct BiomeShaderProperties {
		glm::vec4 albedo_roughness; // rgb = albedo, w = roughness
		glm::vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = unused
	};

	inline static const std::array<BiomeAttributes, 8> kBiomes = {
		// spikeDamp, detailMask, floor, weight, albedo, roughness, metallic, detailStr, detailScale, noiseType, drag, aerosolRate, sensibleHeat, aerosolColor
		BiomeAttributes{
			1.00f,
			0.90f,
			5.00f,
			2.0f,
			glm::vec3(0.76f, 0.70f, 0.55f),
			0.90f,
			0.0f,
			0.10f,
			40.0f,
			1.0f,
			0.1f,
			0.5f,
			0.8f,
			glm::vec3(0.85f, 0.80f, 0.70f)
		}, // Sand (High heat, moderate dust)
		BiomeAttributes{
			0.80f,
			0.50f,
			20.00f,
			10.0f,
			glm::vec3(0.20f, 0.45f, 0.15f),
			0.70f,
			0.0f,
			0.20f,
			20.0f,
			2.0f,
			0.3f,
			0.1f,
			0.4f,
			glm::vec3(0.40f, 0.50f, 0.30f)
		}, // Lush Grass (Moderate drag, low pollen)
		BiomeAttributes{
			0.05f,
			0.60f,
			40.00f,
			5.0f,
			glm::vec3(0.45f, 0.50f, 0.25f),
			0.80f,
			0.0f,
			0.15f,
			15.0f,
			2.0f,
			0.25f,
			0.3f,
			0.6f,
			glm::vec3(0.60f, 0.55f, 0.40f)
		}, // Dry Grass (Lower drag, moderate dust)
		BiomeAttributes{
			0.30f,
			0.50f,
			60.00f,
			15.0f,
			glm::vec3(0.12f, 0.28f, 0.10f),
			0.85f,
			0.0f,
			0.30f,
			10.0f,
			2.0f,
			0.8f,
			0.6f,
			0.3f,
			glm::vec3(0.30f, 0.40f, 0.20f)
		}, // Forest (High drag, high biological aerosol)
		BiomeAttributes{
			0.40f,
			0.40f,
			80.00f,
			9.0f,
			glm::vec3(0.35f, 0.45f, 0.25f),
			0.80f,
			0.0f,
			0.25f,
			15.0f,
			2.0f,
			0.4f,
			0.2f,
			0.4f,
			glm::vec3(0.50f, 0.60f, 0.50f)
		}, // Alpine Meadow (Moderate properties)
		BiomeAttributes{
			0.30f,
			0.20f,
			100.0f,
			5.0f,
			glm::vec3(0.35f, 0.30f, 0.25f),
			0.60f,
			0.0f,
			0.50f,
			5.0f,
			3.0f,
			0.6f,
			0.05f,
			0.7f,
			glm::vec3(0.50f, 0.45f, 0.40f)
		}, // Brown Rock (High drag, high sensible heat)
		BiomeAttributes{
			0.10f,
			0.10f,
			150.0f,
			3.0f,
			glm::vec3(0.45f, 0.45f, 0.48f),
			0.60f,
			0.0f,
			0.40f,
			4.0f,
			3.0f,
			0.6f,
			0.05f,
			0.6f,
			glm::vec3(0.50f, 0.50f, 0.55f)
		}, // Grey Rock (High drag, high sensible heat)
		BiomeAttributes{
			0.05f,
			0.50f,
			250.0f,
			2.0f,
			glm::vec3(0.95f, 0.97f, 1.00f),
			0.40f,
			0.0f,
			0.05f,
			30.0f,
			0.0f,
			0.05f,
			0.01f,
			0.1f,
			glm::vec3(0.90f, 0.95f, 1.00f)
		} // Snow (Low everything except albedo)
	};

} // namespace Boidsish
