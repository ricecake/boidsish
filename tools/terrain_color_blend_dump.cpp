#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
#include <cmath>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>

// Oklab conversion helpers
struct Oklab {
	float L;
	float a;
	float b;
};

// Converts sRGB (0-1) to linear RGB
float srgbToLinear(float c) {
	if (c <= 0.04045f) {
		return c / 12.92f;
	}
	return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Converts linear RGB to sRGB (0-1)
float linearToSrgb(float c) {
	if (c <= 0.0031308f) {
		return 12.92f * c;
	}
	return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

Oklab rgbToOklab(const glm::vec3& rgb) {
	float r_lin = srgbToLinear(rgb.r);
	float g_lin = srgbToLinear(rgb.g);
	float b_lin = srgbToLinear(rgb.b);

	float L_lms = 0.4122214708f * r_lin + 0.5363333363f * g_lin + 0.0514452094f * b_lin;
	float M_lms = 0.2119034982f * r_lin + 0.6806995451f * g_lin + 0.1073969166f * b_lin;
	float S_lms = 0.0883024619f * r_lin + 0.2817188376f * g_lin + 0.6299787005f * b_lin;

	float l_prime = std::cbrt(L_lms);
	float m_prime = std::cbrt(M_lms);
	float s_prime = std::cbrt(S_lms);

	Oklab lab;
	lab.L = 0.2104542553f * l_prime + 0.7936177850f * m_prime - 0.0040720468f * s_prime;
	lab.a = 1.9779984951f * l_prime - 2.4285922050f * m_prime + 0.4505937099f * s_prime;
	lab.b = 0.0259040371f * l_prime + 0.7827717662f * m_prime - 0.8086757660f * s_prime;
	return lab;
}

glm::vec3 oklabToRgb(const Oklab& lab) {
	float l_prime = lab.L + 0.3963377774f * lab.a + 0.2158037573f * lab.b;
	float m_prime = lab.L - 0.1055613458f * lab.a - 0.0638541728f * lab.b;
	float s_prime = lab.L - 0.0894841775f * lab.a - 1.2914855480f * lab.b;

	float L_lms = l_prime * l_prime * l_prime;
	float M_lms = m_prime * m_prime * m_prime;
	float S_lms = s_prime * s_prime * s_prime;

	float r_lin = +4.0767416621f * L_lms - 3.3077115913f * M_lms + 0.2309699292f * S_lms;
	float g_lin = -1.2684380046f * L_lms + 2.6097574011f * M_lms - 0.3413193965f * S_lms;
	float b_lin = -0.0041960863f * L_lms - 0.7034186147f * M_lms + 1.7076147010f * S_lms;

	glm::vec3 rgb;
	rgb.r = linearToSrgb(std::max(0.0f, r_lin));
	rgb.g = linearToSrgb(std::max(0.0f, g_lin));
	rgb.b = linearToSrgb(std::max(0.0f, b_lin));
	return rgb;
}

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
	// Height is the slices (0 to 7) placed horizontally. No black division lines.
	// Inside each slice: Moisture is Left-to-Right (X coordinate, 0 to 7), Roughness is Top-to-Bottom (Y coordinate, 0 to 7).
	int block_size = 16;
	int slice_res = 8 * block_size;
	int out_w = 8 * slice_res; // 8 * 128 = 1024
	int out_h = slice_res;     // 128

	std::vector<uint8_t> out_pixels(out_w * out_h * 3, 0); // RGB

	for (int h_idx = 0; h_idx < 8; ++h_idx) {     // Height slices (slice 0..7)
		float h = h_idx / 7.0f;
		int slice_x_offset = h_idx * slice_res;

		for (int r_idx = 0; r_idx < 8; ++r_idx) { // Roughness rows (0 at top, 7 at bottom)
			float r = r_idx / 7.0f;

			for (int m_idx = 0; m_idx < 8; ++m_idx) { // Moisture columns (0 at left, 7 at right)
				float m = m_idx / 7.0f;

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

				// Draw 16x16 block
				for (int py = 0; py < block_size; ++py) {
					int img_y = r_idx * block_size + py; // r_idx=0 is top, r_idx=7 is bottom
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
		std::cout << "  - The 8 horizontal slices correspond to Height levels (low to high: Beach -> Lowland -> Alpine -> Peak)." << std::endl;
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

	// Expecting 1024x128 image layout
	if (width != 1024 || height != 128) {
		std::cerr << "Warning: Loaded image dimensions are " << width << "x" << height
		          << ", but we expected 1024x128. Sampling might be incorrect!" << std::endl;
	}

	std::vector<uint8_t> texture_data(2048, 255); // 8x8x8 * 4 channels = 2048 bytes

	int block_size = 16;
	int slice_res = 128;

	for (int h_idx = 0; h_idx < 8; ++h_idx) {
		int slice_x_offset = h_idx * slice_res;
		for (int m_idx = 0; m_idx < 8; ++m_idx) {
			for (int r_idx = 0; r_idx < 8; ++r_idx) {

				// Oklab averaging over all pixels in the 16x16 block
				double oklab_l_sum = 0.0;
				double oklab_a_sum = 0.0;
				double oklab_b_sum = 0.0;
				double alpha_sum = 0.0;
				int pixel_count = 0;

				for (int py = 0; py < block_size; ++py) {
					int px_y = r_idx * block_size + py;
					px_y = std::clamp(px_y, 0, height - 1);

					for (int px = 0; px < block_size; ++px) {
						int px_x = slice_x_offset + m_idx * block_size + px;
						px_x = std::clamp(px_x, 0, width - 1);

						int pixel_idx = (px_y * width + px_x) * 4;

						glm::vec3 rgb(
							pixels[pixel_idx + 0] / 255.0f,
							pixels[pixel_idx + 1] / 255.0f,
							pixels[pixel_idx + 2] / 255.0f
						);

						Oklab lab = rgbToOklab(rgb);
						oklab_l_sum += lab.L;
						oklab_a_sum += lab.a;
						oklab_b_sum += lab.b;
						alpha_sum += pixels[pixel_idx + 3];
						pixel_count++;
					}
				}

				Oklab avg_lab;
				avg_lab.L = static_cast<float>(oklab_l_sum / pixel_count);
				avg_lab.a = static_cast<float>(oklab_a_sum / pixel_count);
				avg_lab.b = static_cast<float>(oklab_b_sum / pixel_count);

				glm::vec3 avg_rgb = oklabToRgb(avg_lab);
				uint8_t final_r = static_cast<uint8_t>(std::clamp(avg_rgb.r * 255.0f, 0.0f, 255.0f));
				uint8_t final_g = static_cast<uint8_t>(std::clamp(avg_rgb.g * 255.0f, 0.0f, 255.0f));
				uint8_t final_b = static_cast<uint8_t>(std::clamp(avg_rgb.b * 255.0f, 0.0f, 255.0f));
				uint8_t final_a = static_cast<uint8_t>(std::clamp(alpha_sum / pixel_count, 0.0, 255.0));

				// In 3D texture space:
				// - Coordinate X: Height (h_idx)
				// - Coordinate Y: Moisture (m_idx)
				// - Coordinate Z: Roughness (r_idx)
				// Index = (z_idx * 64 + y_idx * 8 + x_idx) * 4
				int index = (r_idx * 64 + m_idx * 8 + h_idx) * 4;

				texture_data[index + 0] = final_r;
				texture_data[index + 1] = final_g;
				texture_data[index + 2] = final_b;
				texture_data[index + 3] = final_a;
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
	out << "// Pre-populated 8x8x8 RGBA8 color table compiled from " << png_filename << ".\n";
	out << "// Layout: x_idx = Height (0..7), y_idx = Moisture (0..7), z_idx = Roughness (0..7).\n";
	out << "// index = (z_idx * 64 + y_idx * 8 + x_idx) * 4.\n";
	out << "const uint8_t kTerrainColorBlendData[2048] = {\n";

	for (int i = 0; i < 2048; i += 16) {
		out << "\t";
		for (int j = 0; j < 16; ++j) {
			out << static_cast<int>(texture_data[i + j]);
			if (i + j < 2047) {
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
		std::string output_header = "include/terrain_color_blend_data.h";
		if (argc > 2) {
			output_header = argv[2];
		}
		std::cout << "Converting edited image " << input_png << " back into C++ header using Oklab averaging..." << std::endl;
		convertPngToHeader(input_png, output_header);
	} else {
		std::string out_filename = "terrain_color_blend_dump.png";
		generateDefaultPng(out_filename);
	}
	return 0;
}
