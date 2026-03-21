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
		Count = 8
	};

	enum class MetaBiome : uint32_t { Grassland = 0, Desert, Tundra, Count };

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
	};

	struct BiomeShaderProperties {
		glm::vec4 albedo_roughness; // rgb = albedo, w = roughness
		glm::vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = unused
	};

	inline static const std::array<BiomeAttributes, 24> kMetaBiomes = {
		// --- GRASSLAND (0-7) ---
		// spikeDamp, detailMask, floor, weight, albedo, roughness, metallic, detailStr, detailScale
		BiomeAttributes{1.00f, 0.90f, 5.00f, 5.0f, glm::vec3(0.76f, 0.70f, 0.55f), 0.90f, 0.0f, 0.10f, 40.0f},  // Sand
		BiomeAttributes{0.80f, 0.50f, 20.00f, 3.0f, glm::vec3(0.20f, 0.45f, 0.15f), 0.70f, 0.0f, 0.20f, 20.0f}, // Lush Grass
		BiomeAttributes{0.05f, 0.60f, 40.00f, 2.0f, glm::vec3(0.45f, 0.50f, 0.25f), 0.80f, 0.0f, 0.15f, 15.0f}, // Dry Grass
		BiomeAttributes{0.30f, 0.50f, 60.00f, 1.0f, glm::vec3(0.12f, 0.28f, 0.10f), 0.85f, 0.0f, 0.30f, 10.0f}, // Forest
		BiomeAttributes{0.40f, 0.40f, 80.00f, 6.0f, glm::vec3(0.35f, 0.45f, 0.25f), 0.80f, 0.0f, 0.25f, 15.0f}, // Alpine Meadow
		BiomeAttributes{0.30f, 0.20f, 100.0f, 1.0f, glm::vec3(0.35f, 0.30f, 0.25f), 0.60f, 0.0f, 0.50f, 5.0f},  // Brown Rock
		BiomeAttributes{0.10f, 0.10f, 150.0f, 3.0f, glm::vec3(0.45f, 0.45f, 0.48f), 0.60f, 0.0f, 0.40f, 4.0f},  // Grey Rock
		BiomeAttributes{0.05f, 0.50f, 250.0f, 5.0f, glm::vec3(0.95f, 0.97f, 1.00f), 0.40f, 0.0f, 0.05f, 30.0f}, // Snow

		// --- DESERT (8-15) ---
		// Redder tones, higher floor levels, sharper gradients
		BiomeAttributes{1.20f, 0.95f, 15.00f, 5.0f, glm::vec3(0.85f, 0.55f, 0.35f), 0.95f, 0.0f, 0.15f, 45.0f}, // Red Sand
		BiomeAttributes{0.90f, 0.60f, 35.00f, 3.0f, glm::vec3(0.65f, 0.45f, 0.25f), 0.85f, 0.0f, 0.25f, 25.0f}, // Arid Scrub
		BiomeAttributes{0.15f, 0.70f, 55.00f, 2.0f, glm::vec3(0.55f, 0.35f, 0.25f), 0.90f, 0.0f, 0.20f, 20.0f}, // Dry Clay
		BiomeAttributes{0.40f, 0.60f, 75.00f, 1.0f, glm::vec3(0.45f, 0.25f, 0.15f), 0.90f, 0.0f, 0.35f, 12.0f}, // Badlands
		BiomeAttributes{0.50f, 0.50f, 100.0f, 6.0f, glm::vec3(0.65f, 0.35f, 0.20f), 0.85f, 0.0f, 0.30f, 18.0f}, // Mesa
		BiomeAttributes{0.40f, 0.30f, 130.0f, 1.0f, glm::vec3(0.55f, 0.25f, 0.20f), 0.70f, 0.0f, 0.60f, 6.0f},  // Red Rock
		BiomeAttributes{0.20f, 0.20f, 180.0f, 3.0f, glm::vec3(0.50f, 0.35f, 0.30f), 0.70f, 0.0f, 0.50f, 5.0f},  // Iron Rock
		BiomeAttributes{0.10f, 0.60f, 280.0f, 5.0f, glm::vec3(0.98f, 0.90f, 0.85f), 0.50f, 0.0f, 0.10f, 35.0f}, // Peak Dust

		// --- TUNDRA (16-23) ---
		// Bluer/White tones, lower floor levels, different damping
		BiomeAttributes{0.80f, 0.80f, -5.00f, 5.0f, glm::vec3(0.70f, 0.80f, 0.95f), 0.85f, 0.0f, 0.08f, 35.0f}, // Ice
		BiomeAttributes{0.70f, 0.40f, 10.00f, 3.0f, glm::vec3(0.35f, 0.55f, 0.65f), 0.65f, 0.0f, 0.18f, 18.0f}, // Frozen Grass
		BiomeAttributes{0.05f, 0.50f, 25.00f, 2.0f, glm::vec3(0.45f, 0.55f, 0.55f), 0.75f, 0.0f, 0.12f, 12.0f}, // Permafrost
		BiomeAttributes{0.25f, 0.45f, 45.00f, 1.0f, glm::vec3(0.25f, 0.35f, 0.45f), 0.80f, 0.0f, 0.25f, 8.0f},  // Taiga
		BiomeAttributes{0.35f, 0.35f, 65.00f, 6.0f, glm::vec3(0.55f, 0.65f, 0.75f), 0.75f, 0.0f, 0.20f, 14.0f}, // Tundra Ridge
		BiomeAttributes{0.25f, 0.15f, 85.00f, 1.0f, glm::vec3(0.45f, 0.45f, 0.55f), 0.55f, 0.0f, 0.45f, 4.5f},  // Frost Rock
		BiomeAttributes{0.08f, 0.08f, 120.0f, 3.0f, glm::vec3(0.55f, 0.55f, 0.65f), 0.55f, 0.0f, 0.35f, 3.5f},  // Blue Rock
		BiomeAttributes{0.02f, 0.40f, 220.0f, 5.0f, glm::vec3(0.90f, 0.95f, 1.00f), 0.35f, 0.0f, 0.03f, 25.0f} // Deep Snow
	};

	// Alias for backward compatibility
	inline static const std::array<BiomeAttributes, 8>& kBiomes = reinterpret_cast<const std::array<BiomeAttributes, 8>&>(kMetaBiomes);

} // namespace Boidsish
