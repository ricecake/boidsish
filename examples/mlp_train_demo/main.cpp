#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <filesystem>
#include <random>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <cctype>

#include <GL/glew.h>
#include "graphics.h"
#include "shader.h"
#include "mlp_network.h"
#include "IWidget.h"

using namespace Boidsish;

class MlpTrainDemo: public UI::IWidget {
public:
	MlpTrainDemo() {
		// Initialize network layout
		ResetNetwork();

		// Create textures
		CreateTextures();

		// Load Shaders
		LoadShaders();

		// Generate the initial target pattern
		GenerateTargetPattern(0.0f);
	}

	~MlpTrainDemo() override {
		DestroyTextures();
		if (grads_ssbo_ != 0) {
			glDeleteBuffers(1, &grads_ssbo_);
		}
	}

	void ResetNetwork() {
		// Default structure: 2 inputs (U, V), two hidden layers of 16, 4 outputs (RGBA)
		std::vector<int> layers;
		std::vector<int> acts;

		if (dimension_ == 2) {
			layers = { num_inputs_, 16, 16, num_outputs_ };
			// Acts: Sine, Sine, Tanh (or identity/sigmoid depending on final layer)
			acts = { 5, 5, (num_outputs_ == 4) ? 4 : 0 };
		} else {
			layers = { num_inputs_, 16, 16, num_outputs_ };
			acts = { 5, 5, 0 };
		}

		mlp_net_.Initialize(layers, acts);
		mlp_net_.RandomizeWeights();

		// Reset grads SSBO
		size_t params_count = mlp_net_.GetParams().size();
		if (grads_ssbo_ != 0) {
			glDeleteBuffers(1, &grads_ssbo_);
		}
		glGenBuffers(1, &grads_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, grads_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, params_count * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		std::vector<float> zeros(params_count, 0.0f);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, params_count * sizeof(float), zeros.data());
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Clear loss history
		loss_history_.clear();
		loss_history_.resize(100, 0.0f);
		history_idx_ = 0;
	}

	void CreateTextures() {
		DestroyTextures();

		// Target textures
		// 2D: 256x256
		glGenTextures(1, &target_tex_2d_);
		glBindTexture(GL_TEXTURE_2D, target_tex_2d_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// 3D: 64x64x64
		glGenTextures(1, &target_tex_3d_);
		glBindTexture(GL_TEXTURE_3D, target_tex_3d_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, 64, 64, 64, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		// Prediction textures
		glGenTextures(1, &eval_tex_2d_);
		glBindTexture(GL_TEXTURE_2D, eval_tex_2d_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &eval_tex_3d_);
		glBindTexture(GL_TEXTURE_3D, eval_tex_3d_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, 64, 64, 64, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		// Error textures (used for squared error calculation)
		glGenTextures(1, &error_tex_2d_);
		glBindTexture(GL_TEXTURE_2D, error_tex_2d_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
		// Mipmapping enabled for fast loss reduction!
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &error_tex_3d_);
		glBindTexture(GL_TEXTURE_3D, error_tex_3d_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, 64, 64, 64, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		// 3D Slices preview textures (always 2D)
		glGenTextures(1, &preview_target_2d_);
		glBindTexture(GL_TEXTURE_2D, preview_target_2d_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenTextures(1, &preview_eval_2d_);
		glBindTexture(GL_TEXTURE_2D, preview_eval_2d_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenTextures(1, &preview_error_2d_);
		glBindTexture(GL_TEXTURE_2D, preview_error_2d_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void DestroyTextures() {
		if (target_tex_2d_ != 0) glDeleteTextures(1, &target_tex_2d_);
		if (target_tex_3d_ != 0) glDeleteTextures(1, &target_tex_3d_);
		if (eval_tex_2d_ != 0) glDeleteTextures(1, &eval_tex_2d_);
		if (eval_tex_3d_ != 0) glDeleteTextures(1, &eval_tex_3d_);
		if (error_tex_2d_ != 0) glDeleteTextures(1, &error_tex_2d_);
		if (error_tex_3d_ != 0) glDeleteTextures(1, &error_tex_3d_);
		if (preview_target_2d_ != 0) glDeleteTextures(1, &preview_target_2d_);
		if (preview_eval_2d_ != 0) glDeleteTextures(1, &preview_eval_2d_);
		if (preview_error_2d_ != 0) glDeleteTextures(1, &preview_error_2d_);

		target_tex_2d_ = 0;
		target_tex_3d_ = 0;
		eval_tex_2d_ = 0;
		eval_tex_3d_ = 0;
		error_tex_2d_ = 0;
		error_tex_3d_ = 0;
		preview_target_2d_ = 0;
		preview_eval_2d_ = 0;
		preview_error_2d_ = 0;
	}

	void LoadShaders() {
		target_gen_shader_ = std::make_unique<ComputeShader>("shaders/mlp_target_generator.comp");
		train_shader_ = std::make_unique<ComputeShader>("shaders/mlp_train.comp");
		optimizer_shader_ = std::make_unique<ComputeShader>("shaders/mlp_optimizer.comp");
		eval_shader_ = std::make_unique<ComputeShader>("shaders/mlp_eval.comp");
		slice_copy_shader_ = std::make_unique<ComputeShader>("shaders/mlp_slice_copy.comp");

		if (!target_gen_shader_->isValid() || !train_shader_->isValid() ||
			!optimizer_shader_->isValid() || !eval_shader_->isValid() || !slice_copy_shader_->isValid()) {
			std::cerr << "Failed to load one or more MLP training compute shaders!" << std::endl;
		}
	}

	void GenerateTargetPattern(float time) {
		if (!target_gen_shader_ || !target_gen_shader_->isValid()) return;

		target_gen_shader_->use();
		target_gen_shader_->setInt("u_dimension", dimension_);
		target_gen_shader_->setInt("u_pattern_type", pattern_type_);
		target_gen_shader_->setFloat("u_time", time);

		// Bind images
		glBindImageTexture(0, target_tex_2d_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glBindImageTexture(1, target_tex_3d_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

		if (dimension_ == 2) {
			target_gen_shader_->dispatch(256 / 8, 256 / 8, 1);
		} else {
			target_gen_shader_->dispatch(64 / 8, 64 / 8, 64 / 8);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	void RunTrainingEpoch(float time) {
		if (!train_shader_ || !train_shader_->isValid() || !optimizer_shader_ || !optimizer_shader_->isValid()) return;

		// 1. Train step: Calculate loss and accumulate gradients
		train_shader_->use();
		mlp_net_.Bind(57); // Bind MLP params SSBO
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 59, grads_ssbo_); // Gradients SSBO

		// Bind targets as textures for sampling
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, target_tex_2d_);
		train_shader_->setInt("u_target_texture_2d", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, target_tex_3d_);
		train_shader_->setInt("u_target_texture_3d", 1);

		// Bind error images
		glBindImageTexture(2, error_tex_2d_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glBindImageTexture(3, error_tex_3d_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

		train_shader_->setFloat("u_time", time);
		train_shader_->setInt("u_dimension", dimension_);
		train_shader_->setInt("u_num_inputs", num_inputs_);
		train_shader_->setInt("u_num_outputs", num_outputs_);
		train_shader_->setFloat("u_scale", scale_);

		int batch_size = 0;
		if (dimension_ == 2) {
			batch_size = 256 * 256;
			train_shader_->dispatch(256 / 16, 256 / 16, 1);
		} else {
			batch_size = 64 * 64 * 64;
			train_shader_->dispatch(64 / 16, 64 / 16, 64);
		}
		// Include GL_TEXTURE_FETCH_BARRIER_BIT to ensure image writes are visible to texture fetch for mipmap generation
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		// 2. Optimizer step: apply gradients and clear
		optimizer_shader_->use();
		optimizer_shader_->setFloat("u_learning_rate", learning_rate_);
		optimizer_shader_->setFloat("u_batch_size", static_cast<float>(batch_size));

		int params_count = static_cast<int>(mlp_net_.GetParams().size());
		int num_workgroups = (params_count + 255) / 256;
		optimizer_shader_->dispatch(num_workgroups, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 3. Compute loss value via mipmap reduction!
		float mse_loss = 0.0f;
		if (dimension_ == 2) {
			glBindTexture(GL_TEXTURE_2D, error_tex_2d_);
			glGenerateMipmap(GL_TEXTURE_2D);
			float loss_pixel[4] = {0.0f};
			glGetTexImage(GL_TEXTURE_2D, 8, GL_RGBA, GL_FLOAT, loss_pixel); // Level 8 for 256x256
			mse_loss = loss_pixel[0];
		} else {
			glBindTexture(GL_TEXTURE_3D, error_tex_3d_);
			glGenerateMipmap(GL_TEXTURE_3D);
			float loss_pixel[4] = {0.0f};
			glGetTexImage(GL_TEXTURE_3D, 6, GL_RGBA, GL_FLOAT, loss_pixel); // Level 6 for 64x64x64
			mse_loss = loss_pixel[0];
		}

		current_loss_ = mse_loss;
		loss_history_[history_idx_] = mse_loss;
		history_idx_ = (history_idx_ + 1) % loss_history_.size();
	}

	void EvaluateMLP(float time) {
		if (!eval_shader_ || !eval_shader_->isValid()) return;

		eval_shader_->use();
		mlp_net_.Bind(57);

		glBindImageTexture(0, eval_tex_2d_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glBindImageTexture(1, eval_tex_3d_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

		eval_shader_->setFloat("u_time", time);
		eval_shader_->setInt("u_dimension", dimension_);
		eval_shader_->setInt("u_num_inputs", num_inputs_);
		eval_shader_->setInt("u_num_outputs", num_outputs_);
		eval_shader_->setFloat("u_scale", scale_);

		if (dimension_ == 2) {
			eval_shader_->dispatch(256 / 16, 256 / 16, 1);
		} else {
			eval_shader_->dispatch(64 / 16, 64 / 16, 64);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	void UpdateSlices() {
		if (dimension_ != 3 || !slice_copy_shader_ || !slice_copy_shader_->isValid()) return;

		slice_copy_shader_->use();
		slice_copy_shader_->setFloat("u_z_slice", z_slice_);

		// Copy target 3D to 2D
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D, target_tex_3d_);
		glBindImageTexture(0, preview_target_2d_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		slice_copy_shader_->dispatch(256 / 16, 256 / 16, 1);

		// Copy eval 3D to 2D
		glBindTexture(GL_TEXTURE_3D, eval_tex_3d_);
		glBindImageTexture(0, preview_eval_2d_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		slice_copy_shader_->dispatch(256 / 16, 256 / 16, 1);

		// Copy error 3D to 2D
		glBindTexture(GL_TEXTURE_3D, error_tex_3d_);
		glBindImageTexture(0, preview_error_2d_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		slice_copy_shader_->dispatch(256 / 16, 256 / 16, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	void ExportWeights(const std::string& filename) {
		std::ofstream out(filename);
		if (!out.is_open()) {
			std::cerr << "Failed to open file for writing weights: " << filename << std::endl;
			return;
		}

		const auto& meta = mlp_net_.GetMetadata();
		const auto& params = mlp_net_.GetParams();

		out << "#pragma once\n\n";
		out << "// Generated MLP weights exported from MLP Train Sandbox\n";
		out << "namespace Boidsish {\n";
		out << "\tinline const int EXPORTED_NUM_LAYERS = " << meta.num_layers << ";\n";
		out << "\tinline const int EXPORTED_LAYER_SIZES[] = { ";

		out << meta.layers[0].input_size;
		for (int i = 0; i < meta.num_layers; ++i) {
			out << ", " << meta.layers[i].output_size;
		}
		out << " };\n";

		out << "\tinline const int EXPORTED_ACTIVATIONS[] = { ";
		for (int i = 0; i < meta.num_layers; ++i) {
			if (i > 0) out << ", ";
			out << meta.layers[i].activation;
		}
		out << " };\n";

		out << "\tinline const float EXPORTED_PARAMS[] = {\n\t\t";
		for (size_t i = 0; i < params.size(); ++i) {
			if (i > 0) {
				out << ", ";
				if (i % 12 == 0) out << "\n\t\t";
			}
			out << params[i] << "f";
		}
		out << "\n\t};\n";
		out << "} // namespace Boidsish\n";

		out.close();
		std::cout << "Successfully exported weights to: " << filename << std::endl;
	}

	void LoadWeights(const std::string& filename) {
		std::ifstream in(filename);
		if (!in.is_open()) {
			std::cerr << "Failed to open file for loading weights: " << filename << std::endl;
			return;
		}

		// Read parameters back from standard export format
		std::string line;
		std::vector<float> loaded_params;
		std::vector<int> loaded_layer_sizes;
		std::vector<int> loaded_acts;

		bool parsing_params = false;

		while (std::getline(in, line)) {
			if (line.find("EXPORTED_LAYER_SIZES") != std::string::npos) {
				size_t start = line.find("{");
				size_t end = line.find("}");
				if (start != std::string::npos && end != std::string::npos) {
					std::string sizes_str = line.substr(start + 1, end - start - 1);
					std::stringstream ss(sizes_str);
					int size;
					char comma;
					while (ss >> size) {
						loaded_layer_sizes.push_back(size);
						ss >> comma;
					}
				}
			} else if (line.find("EXPORTED_ACTIVATIONS") != std::string::npos) {
				size_t start = line.find("{");
				size_t end = line.find("}");
				if (start != std::string::npos && end != std::string::npos) {
					std::string acts_str = line.substr(start + 1, end - start - 1);
					std::stringstream ss(acts_str);
					int act;
					char comma;
					while (ss >> act) {
						loaded_acts.push_back(act);
						ss >> comma;
					}
				}
			} else if (line.find("EXPORTED_PARAMS") != std::string::npos) {
				parsing_params = true;
				continue;
			} else if (parsing_params) {
				if (line.find("}") != std::string::npos) {
					parsing_params = false;
					continue;
				}
				// Parse floats
				std::stringstream ss(line);
				std::string val_str;
				while (std::getline(ss, val_str, ',')) {
					// remove whitespace and 'f' suffix
					val_str.erase(std::remove_if(val_str.begin(), val_str.end(), [](char c) {
						return std::isspace(c) || c == 'f';
					}), val_str.end());
					if (!val_str.empty()) {
						try {
							loaded_params.push_back(std::stof(val_str));
						} catch (...) {}
					}
				}
			}
		}

		in.close();

		if (!loaded_layer_sizes.empty() && !loaded_params.empty()) {
			num_inputs_ = loaded_layer_sizes.front();
			num_outputs_ = loaded_layer_sizes.back();
			mlp_net_.Initialize(loaded_layer_sizes, loaded_acts);
			std::memcpy(mlp_net_.GetParamsMutable().data(), loaded_params.data(), loaded_params.size() * sizeof(float));
			mlp_net_.SyncToGPU();

			// Reset grads SSBO
			size_t params_count = mlp_net_.GetParams().size();
			if (grads_ssbo_ != 0) {
				glDeleteBuffers(1, &grads_ssbo_);
			}
			glGenBuffers(1, &grads_ssbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, grads_ssbo_);
			glBufferData(GL_SHADER_STORAGE_BUFFER, params_count * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
			std::vector<float> zeros(params_count, 0.0f);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, params_count * sizeof(float), zeros.data());
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			std::cout << "Successfully loaded model weights from: " << filename << std::endl;
		}
	}

	void UpdateAndTrain(float time, float dt) {
		// Update target generator pattern
		GenerateTargetPattern(time);

		// Training steps loop per frame
		if (is_training_) {
			for (int e = 0; e < epochs_per_frame_; ++e) {
				RunTrainingEpoch(time);
			}
		}

		// Run standard evaluation on MLP network to visualize the prediction
		EvaluateMLP(time);

		// If 3D, copy slices for 2D visualization
		if (dimension_ == 3) {
			UpdateSlices();
		}
	}

	void Draw() override {
		ImGui::Begin("MLP GPU Training Sandbox", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Train MLPs on Complex Procedural Patterns entirely on GPU!");
		ImGui::Separator();

		// Left Column: Control Panel
		ImGui::BeginChild("Controls", ImVec2(320, 520), true);

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Training Settings");

		const char* dims[] = { "2D Spatial Grid", "3D Volume Grid" };
		int current_dim_idx = (dimension_ == 2) ? 0 : 1;
		if (ImGui::Combo("Data Dimension", &current_dim_idx, dims, 2)) {
			dimension_ = (current_dim_idx == 0) ? 2 : 3;
			num_inputs_ = dimension_; // update default inputs count
			ResetNetwork();
			CreateTextures();
			GenerateTargetPattern(0.0f);
		}

		ImGui::SliderFloat("Coord Scale", &scale_, 0.1f, 10.0f, "%.1f");

		const char* patterns[] = { "Bark / Wood Grain", "Fluid Flow / Weather", "Procedural Layered Noise", "Vortex Swirl" };
		if (ImGui::Combo("Target Pattern", &pattern_type_, patterns, 4)) {
			GenerateTargetPattern(0.0f);
		}

		ImGui::SliderFloat("Learning Rate", &learning_rate_, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderInt("Epochs / Frame", &epochs_per_frame_, 1, 50);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Network Layout");
		ImGui::Text("Active Inputs: %d, Outputs: %d", num_inputs_, num_outputs_);

		if (ImGui::Button("Reset / Re-initialize Weights")) {
			ResetNetwork();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Action Buttons
		if (is_training_) {
			if (ImGui::Button("Pause Training", ImVec2(140, 0))) {
				is_training_ = false;
			}
		} else {
			if (ImGui::Button("Start Training", ImVec2(140, 0))) {
				is_training_ = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Step Epoch", ImVec2(140, 0))) {
			RunTrainingEpoch(0.0f);
		}

		ImGui::Spacing();
		if (ImGui::Button("Export Weights to Header", ImVec2(290, 30))) {
			ExportWeights("mlp_weights_trained.h");
		}
		if (ImGui::Button("Load Weights from Header", ImVec2(290, 30))) {
			LoadWeights("mlp_weights_trained.h");
		}

		ImGui::EndChild();

		ImGui::SameLine();

		// Right Column: Visualization & Diagnostics
		ImGui::BeginChild("Diagnostics", ImVec2(600, 520), true);

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Training Progress");
		ImGui::Text("Current MSE Loss: %.6f", current_loss_);

		// Draw Loss graph
		std::vector<float> plot_data(loss_history_.size());
		for (size_t i = 0; i < loss_history_.size(); ++i) {
			plot_data[i] = loss_history_[(history_idx_ + i) % loss_history_.size()];
		}
		ImGui::PlotLines("Loss Trend", plot_data.data(), static_cast<int>(plot_data.size()), 0, nullptr, 0.0f, 0.1f, ImVec2(580, 80));

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (dimension_ == 3) {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "3D Volume Slices Preview (Z-Slice: %.2f)", z_slice_);
			ImGui::SliderFloat("Z-Slice", &z_slice_, 0.0f, 1.0f);
		} else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "2D Texture Preview");
		}

		ImGui::Spacing();

		// Show side-by-side images
		GLuint target_tex = (dimension_ == 2) ? target_tex_2d_ : preview_target_2d_;
		GLuint eval_tex = (dimension_ == 2) ? eval_tex_2d_ : preview_eval_2d_;
		GLuint error_tex = (dimension_ == 2) ? error_tex_2d_ : preview_error_2d_;

		ImGui::BeginGroup();
		ImGui::Text("Target Pattern");
		ImGui::Image((void*)(intptr_t)target_tex, ImVec2(180, 180), ImVec2(0, 1), ImVec2(1, 0));
		ImGui::EndGroup();

		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("MLP Predicted");
		ImGui::Image((void*)(intptr_t)eval_tex, ImVec2(180, 180), ImVec2(0, 1), ImVec2(1, 0));
		ImGui::EndGroup();

		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("Squared Error");
		ImGui::Image((void*)(intptr_t)error_tex, ImVec2(180, 180), ImVec2(0, 1), ImVec2(1, 0));
		ImGui::EndGroup();

		ImGui::EndChild();

		ImGui::End();
	}

private:
	MLPNetwork mlp_net_;
	GLuint grads_ssbo_ = 0;

	// Compute Shaders
	std::unique_ptr<ComputeShader> target_gen_shader_;
	std::unique_ptr<ComputeShader> train_shader_;
	std::unique_ptr<ComputeShader> optimizer_shader_;
	std::unique_ptr<ComputeShader> eval_shader_;
	std::unique_ptr<ComputeShader> slice_copy_shader_;

	// Dimension & Sizes
	int dimension_ = 2; // 2 = 2D, 3 = 3D
	int num_inputs_ = 2;
	int num_outputs_ = 4;
	float scale_ = 2.0f;
	int pattern_type_ = 0; // 0 = Bark, 1 = Fluids, 2 = Noise, 3 = Vortex Swirl
	float z_slice_ = 0.5f;

	// Textures
	GLuint target_tex_2d_ = 0;
	GLuint target_tex_3d_ = 0;
	GLuint eval_tex_2d_ = 0;
	GLuint eval_tex_3d_ = 0;
	GLuint error_tex_2d_ = 0;
	GLuint error_tex_3d_ = 0;

	// Preview Slices for 3D Slices Visualizer
	GLuint preview_target_2d_ = 0;
	GLuint preview_eval_2d_ = 0;
	GLuint preview_error_2d_ = 0;

	// Training state
	bool is_training_ = true;
	float learning_rate_ = 0.01f;
	int epochs_per_frame_ = 5;
	float current_loss_ = 0.0f;

	std::vector<float> loss_history_;
	size_t history_idx_ = 0;
};

int main() {
	try {
		Visualizer viz(1280, 720, "MLP Backpropagation & Neural Model Training Engine");

		// Set initial camera
		Camera camera(0.0f, 0.0f, 15.0f, 0.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		// Initialize sandbox widget
		auto sandbox = std::make_shared<MlpTrainDemo>();

		// Add sandbox to the visualizer UI
		viz.AddWidget(sandbox);

		// Set up dynamic simulation update loop
		float elapsed_time = 0.0f;
		viz.AddUpdateHandler([sandbox, &elapsed_time](float time, float dt) {
			float clamped_dt = std::min(dt, 0.1f);
			elapsed_time += clamped_dt;
			sandbox->UpdateAndTrain(elapsed_time, clamped_dt);
		});

		std::cout << "Starting MLP GPU Training Sandbox..." << std::endl;
		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Fatal Exception: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
