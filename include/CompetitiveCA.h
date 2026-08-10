#pragma once

#include <memory>
#include <GL/glew.h>

// Forward declaration of ComputeShader in the global namespace
class ComputeShader;

namespace Boidsish {

	class CompetitiveCA {
	public:
		CompetitiveCA();
		~CompetitiveCA();

		// Prevent copying
		CompetitiveCA(const CompetitiveCA&) = delete;
		CompetitiveCA& operator=(const CompetitiveCA&) = delete;

		// Initialize state textures for the CA (ping-pong)
		void Initialize(int width, int height);

		// Run one step of the cellular automata using the compute shader
		void Step(
			float dt,
			int mode,
			float growth,
			float competition,
			float diffusion,
			float sharpness,
			float fuzziness,
			int radius,
			float time
		);

		// Re-seed the texture with various CPU-based patterns
		void Seed(int pattern, int species_count = 4);

		// Getters for textures and dimensions
		GLuint GetCurrentReadTexture() const { return texture_ids_[current_read_idx_]; }
		GLuint GetCurrentWriteTexture() const { return texture_ids_[1 - current_read_idx_]; }
		int GetWidth() const { return width_; }
		int GetHeight() const { return height_; }

		void SwapTextures();

	private:
		void DestroyTextures();

		GLuint texture_ids_[2] = {0, 0};
		int    current_read_idx_ = 0;
		int    width_ = 0;
		int    height_ = 0;

		std::unique_ptr<ComputeShader> compute_shader_;
	};

} // namespace Boidsish
