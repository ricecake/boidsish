#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <filesystem>
#include <random>

#include <GL/glew.h>
#include "graphics.h"
#include "shader.h"
#include "mlp_network.h"
#include "dot.h"
#include "IWidget.h"

using namespace Boidsish;

// Struct matching shaders/mlp_particles.comp
struct GpuParticle {
	glm::vec4 position; // xyz = position, w = active
	glm::vec4 velocity; // xyz = velocity, w = lifetime
	glm::vec4 color;
};

class MlpNcaDemo: public UI::IWidget {
public:
	MlpNcaDemo() {
		// Initialize networks
		// 1. NCA Network: 12 inputs (4 channels * 3 filters), 16 hidden, 16 hidden, 4 outputs (RGBA updates)
		nca_net_.Initialize({12, 16, 16, 4}, {5, 5, 0}); // Sine, Sine, Identity (SIREN Style NCA is extremely stable!)
		nca_net_.RandomizeWeights();
		nca_net_.CreateStateTextures(128, 128); // 128x128 NCA Grid
		SeedNCA();

		// 2. Texture/SDF Network: 3 inputs (X, Y, Time), 16 hidden, 16 hidden, 4 outputs
		tex_net_.Initialize({3, 16, 16, 4}, {5, 5, 4}); // Sine, Sine, Tanh (classic coordinate MLP)
		tex_net_.RandomizeWeights();
		tex_net_.CreateStateTextures(256, 256); // 256x256 Texture Canvas

		// 3. Particle Network: 3 inputs (3D pos), 16 hidden, 16 hidden, 3 outputs (3D force)
		particle_net_.Initialize({3, 16, 16, 3}, {5, 5, 0});
		particle_net_.RandomizeWeights();

		// Create CPU/GPU Particle buffer
		particles_.resize(max_particles_);
		ResetParticles();

		glGenBuffers(1, &particle_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, particle_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, particles_.size() * sizeof(GpuParticle), particles_.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	~MlpNcaDemo() override {
		if (particle_ssbo_ != 0) {
			glDeleteBuffers(1, &particle_ssbo_);
		}
	}

	void SeedNCA() {
		int w = nca_net_.GetWidth();
		int h = nca_net_.GetHeight();
		std::vector<float> pixels(w * h * 4, 0.0f);

		// Seed with a single active pixel in the center (classic growing NCA)
		int cx = w / 2;
		int cy = h / 2;
		int idx = (cy * w + cx) * 4;
		pixels[idx + 0] = 1.0f; // R
		pixels[idx + 1] = 0.0f; // G
		pixels[idx + 2] = 0.0f; // B
		pixels[idx + 3] = 1.0f; // A

		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, nca_net_.GetTexture(i));
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, pixels.data());
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void RandomizeNCA() {
		int w = nca_net_.GetWidth();
		int h = nca_net_.GetHeight();
		std::vector<float> pixels(w * h * 4, 0.0f);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(0.0f, 1.0f);

		for (int i = 0; i < w * h * 4; ++i) {
			pixels[i] = dis(gen);
		}

		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, nca_net_.GetTexture(i));
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, pixels.data());
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void ClearNCA() {
		int w = nca_net_.GetWidth();
		int h = nca_net_.GetHeight();
		std::vector<float> pixels(w * h * 4, 0.0f);

		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, nca_net_.GetTexture(i));
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, pixels.data());
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void ResetParticles() {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(-5.0f, 5.0f);
		std::uniform_real_distribution<float> life_dis(0.1f, 1.0f);

		for (auto& p : particles_) {
			p.position = glm::vec4(dis(gen), dis(gen), dis(gen), 1.0f); // active
			p.velocity = glm::vec4(dis(gen) * 0.5f, dis(gen) * 0.5f, dis(gen) * 0.5f, life_dis(gen));
			p.color = glm::vec4(1.0f);
		}
	}

	void LoadShaders() {
		nca_shader_ = std::make_unique<ComputeShader>("shaders/mlp_nca.comp");
		if (!nca_shader_->isValid()) {
			std::cerr << "Failed to load mlp_nca.comp shader!" << std::endl;
		}

		tex_shader_ = std::make_unique<ComputeShader>("shaders/mlp_texture.comp");
		if (!tex_shader_->isValid()) {
			std::cerr << "Failed to load mlp_texture.comp shader!" << std::endl;
		}

		particle_shader_ = std::make_unique<ComputeShader>("shaders/mlp_particles.comp");
		if (!particle_shader_->isValid()) {
			std::cerr << "Failed to load mlp_particles.comp shader!" << std::endl;
		}
	}

	void UpdateAndRender(float time, float dt) {
		if (!nca_shader_ || !tex_shader_ || !particle_shader_) {
			LoadShaders();
		}

		// Mode 0: Cellular Automata
		if (active_mode_ == 0 && nca_shader_ && nca_shader_->isValid()) {
			nca_shader_->use();
			nca_net_.Bind(57); // Bind MLP params

			// Bind read state (binding 0)
			nca_net_.BindImage(0, false, 0); // read image
			nca_net_.BindImage(1, true, 1);  // write image

			nca_shader_->setFloat("u_time", time);
			nca_shader_->setFloat("u_step_size", nca_step_size_);
			nca_shader_->setFloat("u_update_probability", nca_prob_);

			int gw = (nca_net_.GetWidth() + 15) / 16;
			int gh = (nca_net_.GetHeight() + 15) / 16;
			nca_shader_->dispatch(gw, gh, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			nca_net_.SwapTextures();
		}

		// Mode 1: Coordinates texture evaluation
		if (active_mode_ == 1 && tex_shader_ && tex_shader_->isValid()) {
			tex_shader_->use();
			tex_net_.Bind(57); // Bind MLP params

			tex_net_.BindImage(0, true, 2); // Bind output texture for writing (we can reuse texture slot 2 as flat output texture)

			tex_shader_->setFloat("u_time", time);
			tex_shader_->setInt("u_mode", tex_mode_);
			tex_shader_->setFloat("u_scale", tex_scale_);

			int gw = (tex_net_.GetWidth() + 15) / 16;
			int gh = (tex_net_.GetHeight() + 15) / 16;
			tex_shader_->dispatch(gw, gh, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		// Mode 2: MLP-guided Particles
		if (active_mode_ == 2 && particle_shader_ && particle_shader_->isValid()) {
			particle_shader_->use();
			particle_net_.Bind(57);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 58, particle_ssbo_);

			particle_shader_->setFloat("u_dt", dt);
			particle_shader_->setFloat("u_time", time);
			particle_shader_->setInt("u_max_particles", max_particles_);
			particle_shader_->setFloat("u_speed_scale", particle_speed_);
			particle_shader_->setFloat("u_influence", particle_influence_);

			int group_count = (max_particles_ + 255) / 256;
			particle_shader_->dispatch(group_count, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			// Download particles back to CPU for trail rendering
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, particle_ssbo_);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, particles_.size() * sizeof(GpuParticle), particles_.data());
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
	}

	// Override standard Draw callback for safely rendering ImGui controls during render phase
	void Draw() override {
		ImGui::Begin("Neural Shader Evaluator");

		ImGui::Text("Explore Multi-Layer Perceptrons in Compute Shaders");
		ImGui::Separator();

		const char* modes[] = { "Neural Cellular Automata", "2D Texture / SDF Mapper", "Particle Vector Field" };
		if (ImGui::Combo("Demo Mode", &active_mode_, modes, 3)) {
			// Trigger resets if needed
		}

		ImGui::Spacing();

		if (active_mode_ == 0) {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Mode: Neural Cellular Automata (NCA)");
			ImGui::Text("Evaluates local Sobel derivatives to drive dynamic growth rules.");
			ImGui::SliderFloat("Step Size", &nca_step_size_, 0.01f, 2.0f);
			ImGui::SliderFloat("Update Prob", &nca_prob_, 0.1f, 1.0f);

			if (ImGui::Button("Randomize Weights")) {
				nca_net_.RandomizeWeights();
			}
			ImGui::SameLine();
			if (ImGui::Button("Grow from Seed")) {
				ClearNCA();
				SeedNCA();
			}
			ImGui::SameLine();
			if (ImGui::Button("Random Noise")) {
				RandomizeNCA();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear")) {
				ClearNCA();
			}

			ImGui::Spacing();
			ImGui::Text("NCA State Visualizer (128x128 Grid):");
			// Render the current read texture of NCA
			ImGui::Image((void*)(intptr_t)nca_net_.GetCurrentReadTexture(), ImVec2(380, 380), ImVec2(0, 1), ImVec2(1, 0));
		}
		else if (active_mode_ == 1) {
			ImGui::TextColored(ImVec4(0, 1, 1, 1), "Mode: 2D Texture / SDF Mapper");
			ImGui::Text("Evaluates f(u, v, t) -> RGB or SDF Distance for every pixel.");

			const char* tex_modes[] = { "Flat Color Fields", "Signed Distance Fields (SDF)" };
			ImGui::Combo("Color Style", &tex_mode_, tex_modes, 2);
			ImGui::SliderFloat("UV Scale", &tex_scale_, 0.1f, 10.0f);

			if (ImGui::Button("Randomize Weights")) {
				tex_net_.RandomizeWeights();
			}

			ImGui::Spacing();
			ImGui::Text("MLP Generated Output (256x256):");
			// Slot 2 corresponds to texture index 0 which we mapped for flat outputs
			ImGui::Image((void*)(intptr_t)tex_net_.GetTexture(0), ImVec2(380, 380), ImVec2(0, 1), ImVec2(1, 0));
		}
		else if (active_mode_ == 2) {
			ImGui::TextColored(ImVec4(1, 0, 1, 1), "Mode: Particle Vector Field");
			ImGui::Text("Maps 3D position to guiding forces to steer particle flow.");
			ImGui::SliderFloat("Speed Scale", &particle_speed_, 0.1f, 10.0f);
			ImGui::SliderFloat("Field Influence", &particle_influence_, 0.01f, 1.0f);

			if (ImGui::Button("Randomize Weights")) {
				particle_net_.RandomizeWeights();
			}
			ImGui::SameLine();
			if (ImGui::Button("Respawn Particles")) {
				ResetParticles();
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, particle_ssbo_);
				glBufferData(GL_SHADER_STORAGE_BUFFER, particles_.size() * sizeof(GpuParticle), particles_.data(), GL_DYNAMIC_DRAW);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}

			ImGui::Spacing();
			ImGui::Text("Active Particles: %d", max_particles_);
			ImGui::Text("These particles are being rendered as trails in the 3D viewport!");
		}

		ImGui::End();
	}

	std::vector<std::shared_ptr<Shape>> GetParticlesAsDots() {
		std::vector<std::shared_ptr<Shape>> shapes;
		if (active_mode_ != 2) return shapes;

		for (size_t i = 0; i < particles_.size(); ++i) {
			const auto& p = particles_[i];
			if (p.position.w <= 0.0) continue;

			auto dot = std::make_shared<Dot>(static_cast<int>(i), p.position.x, p.position.y, p.position.z);
			dot->SetTrailLength(40);
			dot->SetColor(p.color.r, p.color.g, p.color.b);
			shapes.push_back(dot);
		}
		return shapes;
	}

private:
	MLPNetwork nca_net_;
	MLPNetwork tex_net_;
	MLPNetwork particle_net_;

	std::unique_ptr<ComputeShader> nca_shader_;
	std::unique_ptr<ComputeShader> tex_shader_;
	std::unique_ptr<ComputeShader> particle_shader_;

	int active_mode_ = 0; // 0 = NCA, 1 = Texture, 2 = Particles

	// NCA Settings
	float nca_step_size_ = 1.0f;
	float nca_prob_ = 0.5f;

	// Texture Settings
	int tex_mode_ = 0;
	float tex_scale_ = 2.0f;

	// Particle Settings
	int max_particles_ = 250; // Rendering 250 dots with trails looks amazing and runs very fast
	float particle_speed_ = 3.0f;
	float particle_influence_ = 0.15f;
	std::vector<GpuParticle> particles_;
	GLuint particle_ssbo_ = 0;
};

int main() {
	try {
		Visualizer viz(1280, 720, "MLP & Neural Cellular Automata Compute Engine");

		// Set initial camera
		Camera camera(0.0f, 0.0f, 15.0f, 0.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		// Initialize demo runner (which is also our IWidget!)
		auto demo = std::make_shared<MlpNcaDemo>();

		// Add our demo widget safely to the Visualizer UI manager
		viz.AddWidget(demo);

		// Track elapsed time
		float elapsed_time = 0.0f;

		// Set up update callback in the frame loop (strictly for simulation, no ImGui drawing here)
		viz.AddUpdateHandler([demo, &elapsed_time](float time, float dt) {
			// Limit dt to prevent massive simulation jumps during lag spikes
			float clamped_dt = std::min(dt, 0.1f);
			elapsed_time += clamped_dt;
			demo->UpdateAndRender(elapsed_time, clamped_dt);
		});

		// Render the guided particles in the 3D scene using Boidsish Dot function
		viz.SetDotFunction([demo](float time) {
			return demo->GetParticlesAsDots();
		});

		std::cout << "Starting MLP Compute Engine demo..." << std::endl;
		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Fatal Exception: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
