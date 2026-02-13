#include <iostream>
#include <vector>

#include "stb_image_write.h"
#include "terrain_generator.h"
#include <glm/glm.hpp>

// Function to map a value to a color using a gradient
glm::vec3 getColor(float control_value) {
	// A simple gradient from blue to green to brown to white
	std::array<glm::vec3, 6> colors = {
		glm::vec3{0.0f, 0.0f, 0.5f}, // Deep water
		glm::vec3{0.0f, 0.5f, 0.5f}, // Shallow water
		glm::vec3{0.0f, 0.5f, 0.0f}, // Low land
		glm::vec3{0.5f, 0.5f, 0.0f}, // Mid land
		glm::vec3{0.5f, 0.5f, 0.5f},
		glm::vec3{1.0f, 1.0f, 1.0f} // High land (snow)
	};
	// float positions[] = {0.0f, 0.2f, 0.4f, 0.7f, 1.0f};

	auto low_threshold = (floor(control_value * colors.size()) / colors.size());
	auto high_threshold = (ceil(control_value * colors.size()) / colors.size());
	auto low_item = colors[int(floor(control_value * colors.size()))];
	auto high_item = colors[int(ceil(control_value * colors.size()))];
	auto t = glm::smoothstep(low_threshold, high_threshold, control_value);
	return glm::mix(low_item, high_item, t);

	// current.spikeDamping = std::lerp(low_item.spikeDamping, high_item.spikeDamping, t);

	// if (value <= positions[0])
	// 	return colors[0];
	// for (size_t i = 0; i < 4; ++i) {
	// 	if (value < positions[i + 1]) {
	// 		float t = (value - positions[i]) / (positions[i + 1] - positions[i]);
	// 		return glm::mix(colors[i], colors[i + 1], t);
	// 	}
	// }
	// return colors[4];
}

void generateHeightmap(const Boidsish::TerrainGenerator& generator, int width, int height) {
	std::vector<unsigned char> pixels(width * height * 3);
	float                      max_height = generator.GetMaxHeight();

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			float worldX = static_cast<float>(x);
			float worldZ = static_cast<float>(y);

			auto [h, normal] = generator.pointProperties(worldX, worldZ);

			float     normalized_height = std::max(0.0f, std::min(1.0f, h / max_height));
			glm::vec3 color = getColor(normalized_height);

			// Add contour lines
			if (static_cast<int>(h) % 10 == 0) {
				color = {0.0f, 0.0f, 0.0f};
			}

			int index = (y * width + x) * 3;
			pixels[index + 0] = static_cast<unsigned char>(color.r * 255.0f);
			pixels[index + 1] = static_cast<unsigned char>(color.g * 255.0f);
			pixels[index + 2] = static_cast<unsigned char>(color.b * 255.0f);
		}
	}

	stbi_write_png("heightmap.png", width, height, 3, pixels.data(), width * 3);
	std::cout << "Generated heightmap.png" << std::endl;
}

void generateBiomeMap(const Boidsish::TerrainGenerator& generator, int width, int height) {
	std::vector<unsigned char> pixels(width * height * 3);
	glm::vec3                  biome_colors[] = {
        {0.0f, 0.0f, 1.0f}, // Blue
        {0.0f, 1.0f, 0.0f}, // Green
        {1.0f, 1.0f, 0.0f}, // Yellow
        {1.0f, 0.5f, 0.0f}, // Orange
        {1.0f, 0.0f, 0.0f}, // Red
        {1.0f, 0.0f, 1.0f}  // Magenta
    };

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			float worldX = static_cast<float>(x);
			float worldZ = static_cast<float>(y);

			float     control_value = generator.getBiomeControlValue(worldX, worldZ);
			int       biome_index = static_cast<int>(floor(control_value * 6));
			glm::vec3 color = biome_colors[biome_index];

			int index = (y * width + x) * 3;
			pixels[index + 0] = static_cast<unsigned char>(color.r * 255.0f);
			pixels[index + 1] = static_cast<unsigned char>(color.g * 255.0f);
			pixels[index + 2] = static_cast<unsigned char>(color.b * 255.0f);
		}
	}

	stbi_write_png("biome_map.png", width, height, 3, pixels.data(), width * 3);
	std::cout << "Generated biome_map.png" << std::endl;
}

void generateDomainWarpMap(const Boidsish::TerrainGenerator& generator, int width, int height) {
	std::vector<unsigned char> pixels(width * height * 3);

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			float worldX = static_cast<float>(x);
			float worldZ = static_cast<float>(y);

			glm::vec2 warp = generator.getDomainWarp(worldX, worldZ);

			// Map the warp vector to a color
			glm::vec3 color = {(warp.x + 1.0f) * 0.5f, (warp.y + 1.0f) * 0.5f, 0.0f};

			int index = (y * width + x) * 3;
			pixels[index + 0] = static_cast<unsigned char>(color.r * 255.0f);
			pixels[index + 1] = static_cast<unsigned char>(color.g * 255.0f);
			pixels[index + 2] = static_cast<unsigned char>(color.b * 255.0f);
		}
	}

	stbi_write_png("domain_warp.png", width, height, 3, pixels.data(), width * 3);
	std::cout << "Generated domain_warp.png" << std::endl;
}

int main() {
	Boidsish::TerrainGenerator generator;
	const int                  width = 2048;
	const int                  height = 2048;

	generateDomainWarpMap(generator, width, height);
	generateBiomeMap(generator, width, height);
	generateHeightmap(generator, width, height);

	return 0;
}
