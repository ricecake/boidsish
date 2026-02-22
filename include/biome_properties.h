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
		BiomeBitset(uint32_t mask) : bits(mask) {}
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
	};

	struct BiomeShaderProperties {
		glm::vec4 albedo_roughness; // rgb = albedo, w = roughness
		glm::vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = unused
	};

	inline static const std::array<BiomeAttributes, 8> kBiomes = {
		// spikeDamp, detailMask, floor, weight, albedo, roughness, metallic, detailStr, detailScale
		BiomeAttributes{1.00f, 0.90f, 5.00f, 5.0f, glm::vec3(0.76f, 0.70f, 0.55f), 0.90f, 0.0f, 0.10f, 40.0f},  // Sand
		BiomeAttributes{0.80f, 0.50f, 20.00f, 3.0f, glm::vec3(0.20f, 0.45f, 0.15f), 0.70f, 0.0f, 0.20f, 20.0f}, // Lush
	                                                                                                            // Grass
		BiomeAttributes{0.05f, 0.60f, 40.00f, 2.0f, glm::vec3(0.45f, 0.50f, 0.25f), 0.80f, 0.0f, 0.15f, 15.0f}, // Dry
	                                                                                                            // Grass
		BiomeAttributes{
			0.30f,
			0.50f,
			60.00f,
			1.0f,
			glm::vec3(0.12f, 0.28f, 0.10f),
			0.85f,
			0.0f,
			0.30f,
			10.0f
		}, // Forest
		BiomeAttributes{
			0.40f,
			0.40f,
			80.00f,
			6.0f,
			glm::vec3(0.35f, 0.45f, 0.25f),
			0.80f,
			0.0f,
			0.25f,
			15.0f
		}, // Alpine Meadow
		BiomeAttributes{0.30f, 0.20f, 100.0f, 1.0f, glm::vec3(0.35f, 0.30f, 0.25f), 0.60f, 0.0f, 0.50f, 5.0f}, // Brown
	                                                                                                           // Rock
		BiomeAttributes{0.10f, 0.10f, 150.0f, 3.0f, glm::vec3(0.45f, 0.45f, 0.48f), 0.60f, 0.0f, 0.40f, 4.0f}, // Grey
	                                                                                                           // Rock
		BiomeAttributes{0.05f, 0.50f, 250.0f, 5.0f, glm::vec3(0.95f, 0.97f, 1.00f), 0.40f, 0.0f, 0.05f, 30.0f} // Snow
	};

} // namespace Boidsish
