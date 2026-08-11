#include "CompetitiveCA.h"
#include "shader.h"
#include <random>
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>

namespace Boidsish {

	CompetitiveCA::CompetitiveCA() {
	}

	CompetitiveCA::~CompetitiveCA() {
		DestroyTextures();
	}

	void CompetitiveCA::Initialize(int width, int height, int mode) {
		DestroyTextures();

		width_ = width;
		height_ = height;
		current_read_idx_ = 0;

		glGenTextures(2, texture_ids_);
		glGenTextures(1, &display_texture_);

		// Initialize state textures
		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, texture_ids_[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		// Initialize display texture
		glBindTexture(GL_TEXTURE_2D, display_texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glBindTexture(GL_TEXTURE_2D, 0);

		// Seed automatically with random noise on creation
		Seed(0, 4, mode);
	}

	void CompetitiveCA::DestroyTextures() {
		if (texture_ids_[0] != 0) {
			glDeleteTextures(2, texture_ids_);
			texture_ids_[0] = 0;
			texture_ids_[1] = 0;
		}
		if (display_texture_ != 0) {
			glDeleteTextures(1, &display_texture_);
			display_texture_ = 0;
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

		int gw = (width_ + 15) / 16;
		int gh = (height_ + 15) / 16;

		if (mode == 3) {
			// --- TWO-PASS ENERGY CA ---
			// Pass 1: Growth & Competition
			glBindImageTexture(0, GetCurrentReadTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
			glBindImageTexture(1, GetCurrentWriteTexture(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glBindImageTexture(2, display_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			compute_shader_->setInt("u_pass", 1);
			compute_shader_->dispatch(gw, gh, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			SwapTextures();

			// Pass 2: Parent Drain Logic
			glBindImageTexture(0, GetCurrentReadTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
			glBindImageTexture(1, GetCurrentWriteTexture(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glBindImageTexture(2, display_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			compute_shader_->setInt("u_pass", 2);
			compute_shader_->dispatch(gw, gh, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			SwapTextures();

		} else {
			// --- ONE-PASS CA ---
			glBindImageTexture(0, GetCurrentReadTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
			glBindImageTexture(1, GetCurrentWriteTexture(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glBindImageTexture(2, display_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			compute_shader_->setInt("u_pass", 1);
			compute_shader_->dispatch(gw, gh, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			SwapTextures();
		}
	}

	void CompetitiveCA::Seed(int pattern, int species_count, int mode) {
		if (width_ <= 0 || height_ <= 0 || texture_ids_[0] == 0) return;

		std::vector<float> pixels(width_ * height_ * 4, 0.0f);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(0.0f, 1.0f);

		int num_species = std::clamp(species_count, 1, 4);

		if (mode == 3) {
			// Seeding for Energy-Conserving Mode (r = ID, g = Energy, b = Probability, a = Delta)
			if (pattern == 0) {
				// Random noise
				for (int y = 0; y < height_; ++y) {
					for (int x = 0; x < width_; ++x) {
						int idx = (y * width_ + x) * 4;
						if ((gen() % 100) < 15) { // 15% seed density
							int species = (gen() % num_species) + 1; // IDs: 1, 2, 3, 4
							pixels[idx + 0] = static_cast<float>(species);
							pixels[idx + 1] = 1.0f; // starting energy
							pixels[idx + 2] = 0.8f; // growth probability
							pixels[idx + 3] = 0.0f; // delta
						}
					}
				}
			} else if (pattern == 1) {
				// Central seed quadrants
				int cx = width_ / 2;
				int cy = height_ / 2;
				int r = 10;

				for (int y = 0; y < height_; ++y) {
					for (int x = 0; x < width_; ++x) {
						int idx = (y * width_ + x) * 4;
						float dx = static_cast<float>(x - cx);
						float dy = static_cast<float>(y - cy);
						if (dx * dx + dy * dy <= r * r) {
							int species = 1;
							if (dx >= 0 && dy >= 0) species = 1;
							else if (dx < 0 && dy >= 0) species = 2 % num_species + 1;
							else if (dx < 0 && dy < 0) species = 3 % num_species + 1;
							else species = 4 % num_species + 1;

							pixels[idx + 0] = static_cast<float>(species);
							pixels[idx + 1] = 1.0f;
							pixels[idx + 2] = 0.8f;
							pixels[idx + 3] = 0.0f;
						}
					}
				}
			} else if (pattern == 2) {
				// Grid of seeds
				int spacing = 32;
				int r = 2;

				for (int y = spacing; y < height_ - spacing; y += spacing) {
					for (int x = spacing; x < width_ - spacing; x += spacing) {
						int species = (gen() % num_species) + 1;
						for (int dy = -r; dy <= r; ++dy) {
							for (int dx = -r; dx <= r; ++dx) {
								int px = x + dx;
								int py = y + dy;
								if (px >= 0 && px < width_ && py >= 0 && py < height_) {
									int idx = (py * width_ + px) * 4;
									pixels[idx + 0] = static_cast<float>(species);
									pixels[idx + 1] = 1.0f;
									pixels[idx + 2] = 0.8f;
									pixels[idx + 3] = 0.0f;
								}
							}
						}
					}
				}
			}
		} else {
			// Standard seeding (R, G, B, A concentrations)
			if (pattern == 0) {
				for (int y = 0; y < height_; ++y) {
					for (int x = 0; x < width_; ++x) {
						int idx = (y * width_ + x) * 4;
						int species = gen() % num_species;
						pixels[idx + species] = 1.0f;
					}
				}
			} else if (pattern == 1) {
				int cx = width_ / 2;
				int cy = height_ / 2;
				int r = 10;

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
				int spacing = 32;
				int r = 2;

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
			}
		}

		// Upload to both state textures
		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, texture_ids_[i]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGBA, GL_FLOAT, pixels.data());
		}

		// Resolve and upload starting display texture
		std::vector<float> display_pixels(width_ * height_ * 4, 1.0f);

		glm::vec3 color_a = glm::vec3(1.0f, 0.1f, 0.1f);   // Vivid Red
		glm::vec3 color_b = glm::vec3(0.1f, 0.9f, 0.1f);   // Vivid Green
		glm::vec3 color_c = glm::vec3(0.1f, 0.4f, 1.0f);   // Electric Blue
		glm::vec3 color_d = glm::vec3(1.0f, 0.9f, 0.1f);   // Gold/Yellow

		for (int i = 0; i < width_ * height_; ++i) {
			glm::vec3 display_color = glm::vec3(0.05f, 0.05f, 0.08f); // Solid slate background

			if (mode == 3) {
				float id_val = pixels[i * 4 + 0];
				float energy = pixels[i * 4 + 1];
				if (id_val > 0.01f) {
					glm::vec3 active_color = glm::vec3(0.0f);
					if (id_val < 1.5f) active_color = color_a;
					else if (id_val < 2.5f) active_color = color_b;
					else if (id_val < 3.5f) active_color = color_c;
					else active_color = color_d;

					display_color = glm::mix(display_color, active_color, std::clamp(energy, 0.1f, 1.0f));
				}
			} else {
				float r_val = pixels[i * 4 + 0];
				float g_val = pixels[i * 4 + 1];
				float b_val = pixels[i * 4 + 2];
				float a_val = pixels[i * 4 + 3];
				float total = r_val + g_val + b_val + a_val;

				if (total > 0.001f) {
					glm::vec3 active_color = (r_val * color_a + g_val * color_b + b_val * color_c + a_val * color_d) / total;
					display_color = glm::mix(display_color, active_color, std::clamp(total, 0.0f, 1.0f));
				}
			}

			display_pixels[i * 4 + 0] = display_color.x;
			display_pixels[i * 4 + 1] = display_color.y;
			display_pixels[i * 4 + 2] = display_color.z;
			display_pixels[i * 4 + 3] = 1.0f; // fully opaque visualizer background!
		}

		glBindTexture(GL_TEXTURE_2D, display_texture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGBA, GL_FLOAT, display_pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

} // namespace Boidsish
