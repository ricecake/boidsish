#include "CompetitiveCA.h"
#include "shader.h"
#include <random>
#include <cmath>
#include <iostream>
#include <vector>

namespace Boidsish {

	CompetitiveCA::CompetitiveCA() {
	}

	CompetitiveCA::~CompetitiveCA() {
		DestroyTextures();
	}

	void CompetitiveCA::Initialize(int width, int height) {
		DestroyTextures();

		width_ = width;
		height_ = height;
		current_read_idx_ = 0;

		glGenTextures(2, texture_ids_);

		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, texture_ids_[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// Seed automatically with random noise on creation
		Seed(0, 4);
	}

	void CompetitiveCA::DestroyTextures() {
		if (texture_ids_[0] != 0) {
			glDeleteTextures(2, texture_ids_);
			texture_ids_[0] = 0;
			texture_ids_[1] = 0;
		}
	}

	void CompetitiveCA::SwapTextures() {
		current_read_idx_ = 1 - current_read_idx_;
	}

	void CompetitiveCA::Step(
		float dt,
		int mode,
		float growth,
		float competition,
		float diffusion,
		float sharpness,
		float fuzziness,
		int radius,
		float time
	) {
		if (!compute_shader_ || !compute_shader_->isValid()) {
			compute_shader_ = std::make_unique<ComputeShader>("shaders/competitive_ca.comp");
			if (!compute_shader_->isValid()) {
				std::cerr << "Error: Failed to load competitive_ca.comp compute shader!" << std::endl;
				return;
			}
		}

		compute_shader_->use();

		// Bind textures as images
		// Unit 0: Read (readonly)
		glBindImageTexture(0, GetCurrentReadTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
		// Unit 1: Write (writeonly)
		glBindImageTexture(1, GetCurrentWriteTexture(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

		// Set uniforms
		compute_shader_->setInt("u_mode", mode);
		compute_shader_->setFloat("u_dt", dt);
		compute_shader_->setFloat("u_growth", growth);
		compute_shader_->setFloat("u_competition", competition);
		compute_shader_->setFloat("u_diffusion", diffusion);
		compute_shader_->setFloat("u_sharpness", sharpness);
		compute_shader_->setFloat("u_fuzziness", fuzziness);
		compute_shader_->setInt("u_radius", radius);
		compute_shader_->setFloat("u_time", time);

		// Dispatch
		int gw = (width_ + 15) / 16;
		int gh = (height_ + 15) / 16;
		compute_shader_->dispatch(gw, gh, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		SwapTextures();
	}

	void CompetitiveCA::Seed(int pattern, int species_count) {
		if (width_ <= 0 || height_ <= 0 || texture_ids_[0] == 0) return;

		std::vector<float> pixels(width_ * height_ * 4, 0.0f);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(0.0f, 1.0f);

		// Clamp species count between 1 and 4
		int num_species = std::clamp(species_count, 1, 4);

		if (pattern == 0) {
			// Pattern 0: Random noise
			for (int y = 0; y < height_; ++y) {
				for (int x = 0; x < width_; ++x) {
					int idx = (y * width_ + x) * 4;
					int species = gen() % num_species;
					pixels[idx + species] = 1.0f;
				}
			}
		} else if (pattern == 1) {
			// Pattern 1: Central seed quadrants
			int cx = width_ / 2;
			int cy = height_ / 2;
			int r = 10; // 10-pixel starting radius

			for (int y = 0; y < height_; ++y) {
				for (int x = 0; x < width_; ++x) {
					int idx = (y * width_ + x) * 4;
					float dx = static_cast<float>(x - cx);
					float dy = static_cast<float>(y - cy);
					if (dx * dx + dy * dy <= r * r) {
						if (dx >= 0 && dy >= 0) {
							pixels[idx + 0] = 1.0f;
						} else if (dx < 0 && dy >= 0) {
							pixels[idx + 1 % num_species] = 1.0f;
						} else if (dx < 0 && dy < 0) {
							pixels[idx + 2 % num_species] = 1.0f;
						} else {
							pixels[idx + 3 % num_species] = 1.0f;
						}
					}
				}
			}
		} else if (pattern == 2) {
			// Pattern 2: Grid of seeds
			int spacing = 32;
			int r = 2; // small seed radius

			for (int y = spacing; y < height_ - spacing; y += spacing) {
				for (int x = spacing; x < width_ - spacing; x += spacing) {
					int species = gen() % num_species;
					for (int dy = -r; dy <= r; ++dy) {
						for (int dx = -r; dx <= r; ++dx) {
							int px = x + dx;
							int py = y + dy;
							if (px >= 0 && px < width_ && py >= 0 && py < height_) {
								int idx = (py * width_ + px) * 4;
								pixels[idx + species] = 1.0f;
							}
						}
					}
				}
			}
		} else if (pattern == 3) {
			// Pattern 3: Clear
			// Already initialized to all 0.0f
		}

		// Upload to both textures
		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, texture_ids_[i]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGBA, GL_FLOAT, pixels.data());
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}

} // namespace Boidsish
