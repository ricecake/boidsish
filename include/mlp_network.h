#pragma once

#include <vector>
#include <string>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	struct LayerInfo {
		int input_size;
		int output_size;
		int weight_offset;
		int bias_offset;
		int activation; // 0 = Identity, 1 = ReLU, 2 = LeakyReLU, 3 = Sigmoid, 4 = Tanh, 5 = Sine
	};

	struct MLPMetadata {
		int num_layers = 0;
		int max_layer_size = 0;
		LayerInfo layers[16] = {};
	};

	class MLPNetwork {
	public:
		MLPNetwork();
		~MLPNetwork();

		// Prevent copying
		MLPNetwork(const MLPNetwork&) = delete;
		MLPNetwork& operator=(const MLPNetwork&) = delete;

		// Support move semantics
		MLPNetwork(MLPNetwork&& other) noexcept;
		MLPNetwork& operator=(MLPNetwork&& other) noexcept;

		// Initialize network layout. e.g. layer_sizes = {2, 16, 16, 4}, activations = {5, 5, 0}
		void Initialize(const std::vector<int>& layer_sizes, const std::vector<int>& activations);

		// Randomize weights/biases using Xavier (Glorot) initialization.
		void RandomizeWeights();

		// Upload parameters and metadata to GPU SSBO
		void SyncToGPU();

		// Bind the SSBO to the specified binding point. Default is 57.
		void Bind(GLuint binding_point = 57) const;

		// Texture management helpers for ping-ponging or flat outputs.
		void CreateStateTextures(int width, int height, GLenum internal_format = GL_RGBA32F);
		void DestroyTextures();
		void SwapTextures();

		// Image and Texture bindings helper
		void BindImage(GLuint unit, bool write_only, int texture_idx = 0) const;
		void BindTexture(GLuint unit, int texture_idx = 0) const;

		// Getters
		GLuint GetSSBO() const { return ssbo_id_; }
		GLuint GetTexture(int idx) const { return texture_ids_[idx]; }
		GLuint GetCurrentReadTexture() const { return texture_ids_[current_read_idx_]; }
		GLuint GetCurrentWriteTexture() const { return texture_ids_[1 - current_read_idx_]; }
		int GetWidth() const { return width_; }
		int GetHeight() const { return height_; }
		const MLPMetadata& GetMetadata() const { return metadata_; }
		const std::vector<float>& GetParams() const { return params_; }
		std::vector<float>& GetParamsMutable() { return params_; }

	private:
		MLPMetadata        metadata_;
		std::vector<float> params_;

		GLuint ssbo_id_ = 0;
		GLuint texture_ids_[2] = {0, 0};
		int    current_read_idx_ = 0;
		int    width_ = 0;
		int    height_ = 0;
	};

} // namespace Boidsish
