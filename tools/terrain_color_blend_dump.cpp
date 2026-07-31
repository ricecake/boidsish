#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>

void generateDefaultPng(const std::string& out_filename) {
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

	// Layout parameters
	// Height is the slices (0 to 3) placed horizontally.
	// Inside each slice: Moisture is Left-to-Right (X coordinate, 0 to 3), Roughness is Top-to-Bottom (Y coordinate, 0 to 3).
	int block_size = 32;
	int slice_res = 4 * block_size;
	int border_size = 2;
	int out_w = 4 * slice_res + 3 * border_size;
	int out_h = slice_res;

	std::vector<uint8_t> out_pixels(out_w * out_h * 3, 0); // RGB

	for (int h_idx = 0; h_idx < 4; ++h_idx) {     // Height slices (Z-like, slice 0..3)
		float h = h_idx / 3.0f;
		int slice_x_offset = h_idx * (slice_res + border_size);

		for (int r_idx = 0; r_idx < 4; ++r_idx) { // Roughness rows (0 at top, 3 at bottom)
			float r = r_idx / 3.0f;

			for (int m_idx = 0; m_idx < 4; ++m_idx) { // Moisture columns (0 at left, 3 at right)
				float m = m_idx / 3.0f;

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

				uint8_t col_r = static_cast<uint8_t>(std::clamp(finalColor.r * 255.0f, 0.0f, 255.0f));
				uint8_t col_g = static_cast<uint8_t>(std::clamp(finalColor.g * 255.0f, 0.0f, 255.0f));
				uint8_t col_b = static_cast<uint8_t>(std::clamp(finalColor.b * 255.0f, 0.0f, 255.0f));

				// Draw 32x32 block
				for (int py = 0; py < block_size; ++py) {
					int img_y = r_idx * block_size + py; // r_idx=0 is top, r_idx=3 is bottom
					for (int px = 0; px < block_size; ++px) {
						int img_x = slice_x_offset + m_idx * block_size + px;

						int out_idx = (img_y * out_w + img_x) * 3;
						out_pixels[out_idx + 0] = col_r;
						out_pixels[out_idx + 1] = col_g;
						out_pixels[out_idx + 2] = col_b;
					}
				}
			}
		}
	}

	if (stbi_write_png(out_filename.c_str(), out_w, out_h, 3, out_pixels.data(), out_w * 3)) {
		std::cout << "Successfully exported " << out_filename << " (" << out_w << "x" << out_h << " pixels)." << std::endl;
		std::cout << "Axes mapping in output image:" << std::endl;
		std::cout << "  - Inside each slice: Moisture is Left-to-Right (dry to wet)." << std::endl;
		std::cout << "                       Roughness is Top-to-Bottom (smooth to rough)." << std::endl;
		std::cout << "  - The 4 horizontal slices correspond to Height levels (low to high: Beach -> Lowland -> Alpine -> Peak)." << std::endl;
	} else {
		std::cerr << "Failed to write " << out_filename << std::endl;
	}
}

void convertPngToHeader(const std::string& png_filename, const std::string& out_header) {
	int width, height, channels;
	uint8_t* pixels = stbi_load(png_filename.c_str(), &width, &height, &channels, 4);
	if (!pixels) {
		std::cerr << "Error: Failed to load " << png_filename << std::endl;
		return;
	}

	// Expecting 518x128 image layout
	if (width != 518 || height != 128) {
		std::cerr << "Warning: Loaded image dimensions are " << width << "x" << height
		          << ", but we expected 518x128. Sampling might be incorrect!" << std::endl;
	}

	std::vector<uint8_t> texture_data(256, 255); // 4x4x4 * 4 channels = 256 bytes

	int block_size = 32;
	int border_size = 2;
	int slice_res = 128;

	for (int h_idx = 0; h_idx < 4; ++h_idx) {
		int slice_x_offset = h_idx * (slice_res + border_size);
		for (int m_idx = 0; m_idx < 4; ++m_idx) {
			for (int r_idx = 0; r_idx < 4; ++r_idx) {
				// Sample the center of the block
				int sample_x = slice_x_offset + m_idx * block_size + block_size / 2;
				int sample_y = r_idx * block_size + block_size / 2;

				// Clamp coordinates just in case
				sample_x = std::clamp(sample_x, 0, width - 1);
				sample_y = std::clamp(sample_y, 0, height - 1);

				int pixel_idx = (sample_y * width + sample_x) * 4;

				// In 3D texture space:
				// - Coordinate X: Height (h_idx)
				// - Coordinate Y: Moisture (m_idx)
				// - Coordinate Z: Roughness (r_idx)
				// Index = (z_idx * 16 + y_idx * 4 + x_idx) * 4
				int index = (r_idx * 16 + m_idx * 4 + h_idx) * 4;

				texture_data[index + 0] = pixels[pixel_idx + 0];
				texture_data[index + 1] = pixels[pixel_idx + 1];
				texture_data[index + 2] = pixels[pixel_idx + 2];
				texture_data[index + 3] = pixels[pixel_idx + 3];
			}
		}
	}

	stbi_image_free(pixels);

	// Write C++ Header
	std::ofstream out(out_header);
	if (!out.is_open()) {
		std::cerr << "Error: Failed to open output header file " << out_header << std::endl;
		return;
	}

	out << "#pragma once\n";
	out << "#include <stdint.h>\n\n";
	out << "// Pre-populated 4x4x4 RGBA8 color table compiled from " << png_filename << ".\n";
	out << "// Layout: x_idx = Height (0..3), y_idx = Moisture (0..3), z_idx = Roughness (0..3).\n";
	out << "// index = (z_idx * 16 + y_idx * 4 + x_idx) * 4.\n";
	out << "const uint8_t kTerrainColorBlendData[256] = {\n";

	for (int i = 0; i < 256; i += 16) {
		out << "\t";
		for (int j = 0; j < 16; ++j) {
			out << static_cast<int>(texture_data[i + j]);
			if (i + j < 255) {
				out << ", ";
			}
		}
		out << "\n";
	}
	out << "};\n";
	out.close();

	std::cout << "Successfully generated " << out_header << " containing kTerrainColorBlendData array." << std::endl;
}

int main(int argc, char** argv) {
	if (argc > 1) {
		std::string input_png = argv[1];
		std::string output_header = "terrain_color_blend_data.h";
		if (argc > 2) {
			output_header = argv[2];
		}
		std::cout << "Converting edited image " << input_png << " back into C++ header..." << std::endl;
		convertPngToHeader(input_png, output_header);
	} else {
		std::string out_filename = "terrain_color_blend_dump.png";
		generateDefaultPng(out_filename);
	}
	return 0;
}
