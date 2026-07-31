#include <iostream>
#include <vector>
#include <algorithm>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <glm/glm.hpp>

int main() {
	// Colors defined in the terrain fragment shader
	const glm::vec3 COL_SAND_WET = glm::vec3(0.55f, 0.45f, 0.35f);
	const glm::vec3 COL_SAND_DRY = glm::vec3(0.76f, 0.70f, 0.55f);
	const glm::vec3 COL_GRASS_LUSH = glm::vec3(0.20f, 0.45f, 0.15f);
	const glm::vec3 COL_GRASS_DRY = glm::vec3(0.45f, 0.50f, 0.25f);
	const glm::vec3 COL_FOREST = glm::vec3(0.12f, 0.28f, 0.10f);
	const glm::vec3 COL_ALPINE_MEADOW = glm::vec3(0.35f, 0.45f, 0.25f);
	const glm::vec3 COL_ROCK_BROWN = glm::vec3(0.35f, 0.30f, 0.25f);
	const glm::vec3 COL_ROCK_GREY = glm::vec3(0.45f, 0.45f, 0.48f);
	const glm::vec3 COL_ROCK_DARK = glm::vec3(0.25f, 0.23f, 0.22f);
	const glm::vec3 COL_SNOW_FRESH = glm::vec3(0.95f, 0.97f, 1.00f);
	const glm::vec3 COL_SNOW_OLD = glm::vec3(0.85f, 0.88f, 0.92f);
	const glm::vec3 COL_DIRT = glm::vec3(0.35f, 0.25f, 0.18f);

	// Generate 4x4x4 3D data representing the color blend texture
	std::vector<glm::vec3> blend_colors(4 * 4 * 4);
	for (int z = 0; z < 4; ++z) {     // Roughness (Z)
		float r = z / 3.0f;
		for (int y = 0; y < 4; ++y) { // Moisture (Y)
			float m = y / 3.0f;
			for (int x = 0; x < 4; ++x) { // Height (X)
				float h = x / 3.0f;

				// Compute beachColor (Band 0)
				float wetness = m * (1.0f - r);
				glm::vec3 beachColor = glm::mix(COL_SAND_DRY, COL_SAND_WET, wetness);
				beachColor = glm::mix(beachColor, COL_ROCK_DARK, (1.0f - r) * m * 0.5f);

				// Compute lowlandColor (Band 1)
				glm::vec3 lushColor = glm::mix(COL_GRASS_LUSH, COL_FOREST, m);
				glm::vec3 dryColor = glm::mix(COL_DIRT, COL_GRASS_DRY, m);
				glm::vec3 grassColor = glm::mix(dryColor, lushColor, m);
				glm::vec3 smoothColor = glm::mix(COL_ROCK_DARK, COL_DIRT, m);
				glm::vec3 lowlandColor = glm::mix(smoothColor, grassColor, r);

				// Compute alpineColor (Band 2)
				glm::vec3 rockColor = glm::mix(COL_ROCK_BROWN, COL_ROCK_GREY, m);
				glm::vec3 alpineMeadow = COL_ALPINE_MEADOW;
				glm::vec3 roughAlpine = glm::mix(rockColor, alpineMeadow, m);
				glm::vec3 smoothRock = COL_ROCK_DARK;
				glm::vec3 alpineColor = glm::mix(smoothRock, roughAlpine, r);

				// Compute snowColor (Band 3)
				glm::vec3 snowColor = glm::mix(COL_SNOW_OLD, COL_SNOW_FRESH, r);

				// Blend bands based on height h
				glm::vec3 finalColor;
				if (h < 0.333f) {
					float t = h / 0.333f;
					finalColor = glm::mix(beachColor, lowlandColor, t);
				} else if (h < 0.666f) {
					float t = (h - 0.333f) / 0.333f;
					finalColor = glm::mix(lowlandColor, alpineColor, t);
				} else {
					float t = (h - 0.666f) / 0.334f;
					finalColor = glm::mix(alpineColor, snowColor, t);
				}

				blend_colors[z * 16 + y * 4 + x] = finalColor;
			}
		}
	}

	// Layout parameters
	// Each 4x4 layer (slice along roughness axis Z) will be upscaled to 32x32 per-pixel.
	// So each slice is 128x128 pixels.
	// We'll place the 4 roughness slices horizontally: Slice 0, Slice 1, Slice 2, Slice 3.
	// We will add a 2-pixel black border between the slices.
	// Total width: 4 * 128 + 3 * 2 = 518 pixels.
	// Total height: 128 pixels.
	int block_size = 32;
	int slice_res = 4 * block_size;
	int border_size = 2;
	int out_w = 4 * slice_res + 3 * border_size;
	int out_h = slice_res;

	std::vector<uint8_t> out_pixels(out_w * out_h * 3, 0); // RGB

	for (int z = 0; z < 4; ++z) {
		int slice_x_offset = z * (slice_res + border_size);
		for (int y = 0; y < 4; ++y) {
			for (int x = 0; x < 4; ++x) {
				glm::vec3 col = blend_colors[z * 16 + y * 4 + x];
				uint8_t r = static_cast<uint8_t>(std::clamp(col.r * 255.0f, 0.0f, 255.0f));
				uint8_t g = static_cast<uint8_t>(std::clamp(col.g * 255.0f, 0.0f, 255.0f));
				uint8_t b = static_cast<uint8_t>(std::clamp(col.b * 255.0f, 0.0f, 255.0f));

				// Fill block of size block_size x block_size
				// Moisture (Y) increases from bottom to top, and Height (X) increases from left to right.
				for (int py = 0; py < block_size; ++py) {
					int img_y = (3 - y) * block_size + py; // Invert Y so bottom is y=0
					for (int px = 0; px < block_size; ++px) {
						int img_x = slice_x_offset + x * block_size + px;

						int out_idx = (img_y * out_w + img_x) * 3;
						out_pixels[out_idx + 0] = r;
						out_pixels[out_idx + 1] = g;
						out_pixels[out_idx + 2] = b;
					}
				}
			}
		}
	}

	std::string out_filename = "terrain_color_blend_dump.png";
	if (stbi_write_png(out_filename.c_str(), out_w, out_h, 3, out_pixels.data(), out_w * 3)) {
		std::cout << "Successfully exported " << out_filename << " (" << out_w << "x" << out_h << " pixels)." << std::endl;
		std::cout << "Axes: X inside each slice represents Height (left to right: low to high)." << std::endl;
		std::cout << "      Y inside each slice represents Moisture (bottom to top: dry to wet)." << std::endl;
		std::cout << "      The 4 separate horizontal slices represent Roughness (left to right: smooth to rough)." << std::endl;
	} else {
		std::cerr << "Failed to write " << out_filename << std::endl;
		return 1;
	}

	return 0;
}
