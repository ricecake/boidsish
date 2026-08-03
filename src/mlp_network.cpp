#include "mlp_network.h"
#include <random>
#include <cmath>
#include <cstring>
#include <iostream>

namespace Boidsish {

	MLPNetwork::MLPNetwork() : ssbo_id_(0) {
	}

	MLPNetwork::~MLPNetwork() {
		if (ssbo_id_ != 0) {
			glDeleteBuffers(1, &ssbo_id_);
		}
		DestroyTextures();
	}

	MLPNetwork::MLPNetwork(MLPNetwork&& other) noexcept :
		metadata_(other.metadata_),
		params_(std::move(other.params_)),
		ssbo_id_(other.ssbo_id_),
		current_read_idx_(other.current_read_idx_),
		width_(other.width_),
		height_(other.height_) {
		texture_ids_[0] = other.texture_ids_[0];
		texture_ids_[1] = other.texture_ids_[1];

		other.ssbo_id_ = 0;
		other.texture_ids_[0] = 0;
		other.texture_ids_[1] = 0;
	}

	MLPNetwork& MLPNetwork::operator=(MLPNetwork&& other) noexcept {
		if (this != &other) {
			if (ssbo_id_ != 0) {
				glDeleteBuffers(1, &ssbo_id_);
			}
			DestroyTextures();

			metadata_ = other.metadata_;
			params_ = std::move(other.params_);
			ssbo_id_ = other.ssbo_id_;
			texture_ids_[0] = other.texture_ids_[0];
			texture_ids_[1] = other.texture_ids_[1];
			current_read_idx_ = other.current_read_idx_;
			width_ = other.width_;
			height_ = other.height_;

			other.ssbo_id_ = 0;
			other.texture_ids_[0] = 0;
			other.texture_ids_[1] = 0;
		}
		return *this;
	}

	void MLPNetwork::Initialize(const std::vector<int>& layer_sizes, const std::vector<int>& activations) {
		if (layer_sizes.size() < 2) {
			std::cerr << "MLPNetwork Error: layer_sizes must contain at least 2 layers (input and output)." << std::endl;
			return;
		}

		int num_layers = static_cast<int>(layer_sizes.size() - 1);
		if (num_layers > 16) {
			std::cerr << "MLPNetwork Error: Maximum supported layers is 16." << std::endl;
			num_layers = 16;
		}

		metadata_.num_layers = num_layers;
		metadata_.max_layer_size = 0;

		params_.clear();

		for (int i = 0; i < num_layers; ++i) {
			LayerInfo info;
			info.input_size = layer_sizes[i];
			info.output_size = layer_sizes[i + 1];
			info.activation = (i < static_cast<int>(activations.size())) ? activations[i] : 0; // default to Identity

			metadata_.max_layer_size = std::max(metadata_.max_layer_size, std::max(info.input_size, info.output_size));

			// Sequential allocation in the flat params array
			info.weight_offset = static_cast<int>(params_.size());
			params_.insert(params_.end(), info.input_size * info.output_size, 0.0f);

			info.bias_offset = static_cast<int>(params_.size());
			params_.insert(params_.end(), info.output_size, 0.0f);

			metadata_.layers[i] = info;
		}

		SyncToGPU();
	}

	void MLPNetwork::RandomizeWeights() {
		std::random_device rd;
		std::mt19937 gen(rd());

		for (int l = 0; l < metadata_.num_layers; ++l) {
			const auto& layer = metadata_.layers[l];

			// Xavier / Glorot Initialization
			float limit = std::sqrt(6.0f / (layer.input_size + layer.output_size));
			std::uniform_real_distribution<float> dis(-limit, limit);

			// Populate weights
			for (int i = 0; i < layer.input_size * layer.output_size; ++i) {
				params_[layer.weight_offset + i] = dis(gen);
			}

			// Populate biases to 0 or very small noise
			std::uniform_real_distribution<float> bias_dis(-0.01f, 0.01f);
			for (int i = 0; i < layer.output_size; ++i) {
				params_[layer.bias_offset + i] = bias_dis(gen);
			}
		}

		SyncToGPU();
	}

	void MLPNetwork::SyncToGPU() {
		if (ssbo_id_ == 0) {
			glGenBuffers(1, &ssbo_id_);
		}
		if (ssbo_id_ == 0) return;

		size_t metadata_size = sizeof(MLPMetadata);
		size_t params_size = params_.size() * sizeof(float);
		size_t total_size = metadata_size + params_size;

		std::vector<uint8_t> buffer_data(total_size);
		std::memcpy(buffer_data.data(), &metadata_, metadata_size);
		if (params_size > 0) {
			std::memcpy(buffer_data.data() + metadata_size, params_.data(), params_size);
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, total_size, buffer_data.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void MLPNetwork::Bind(GLuint binding_point) const {
		if (ssbo_id_ == 0) {
			// Const cast to allow lazy initialization during bind
			glGenBuffers(1, const_cast<GLuint*>(&ssbo_id_));
		}
		if (ssbo_id_ == 0) return;
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, ssbo_id_);
	}

	void MLPNetwork::CreateStateTextures(int width, int height, GLenum internal_format) {
		DestroyTextures();

		width_ = width;
		height_ = height;
		current_read_idx_ = 0;

		glGenTextures(2, texture_ids_);

		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, texture_ids_[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void MLPNetwork::DestroyTextures() {
		if (texture_ids_[0] != 0) {
			glDeleteTextures(2, texture_ids_);
			texture_ids_[0] = 0;
			texture_ids_[1] = 0;
		}
	}

	void MLPNetwork::SwapTextures() {
		current_read_idx_ = 1 - current_read_idx_;
	}

	void MLPNetwork::BindImage(GLuint unit, bool write_only, int texture_idx) const {
		GLuint tex_id = 0;
		if (texture_idx == 0) tex_id = GetCurrentReadTexture();
		else if (texture_idx == 1) tex_id = GetCurrentWriteTexture();
		else tex_id = texture_ids_[texture_idx - 2];

		glBindImageTexture(unit, tex_id, 0, GL_FALSE, 0, write_only ? GL_WRITE_ONLY : GL_READ_WRITE, GL_RGBA32F);
	}

	void MLPNetwork::BindTexture(GLuint unit, int texture_idx) const {
		GLuint tex_id = 0;
		if (texture_idx == 0) tex_id = GetCurrentReadTexture();
		else if (texture_idx == 1) tex_id = GetCurrentWriteTexture();
		else tex_id = texture_ids_[texture_idx - 2];

		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, tex_id);
	}

} // namespace Boidsish
