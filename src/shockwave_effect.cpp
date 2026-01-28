#include "shockwave_effect.h"

#include <algorithm>

#include "logger.h"
#include "shader.h"
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	ShockwaveManager::ShockwaveManager() {
		shockwaves_.reserve(kMaxShockwaves);
	}

	ShockwaveManager::~ShockwaveManager() {
		if (ubo_ != 0) {
			glDeleteBuffers(1, &ubo_);
		}
		if (fbo_ != 0) {
			glDeleteFramebuffers(1, &fbo_);
		}
		if (output_texture_ != 0) {
			glDeleteTextures(1, &output_texture_);
		}
	}

	void ShockwaveManager::Initialize(int screen_width, int screen_height) {
		if (initialized_) {
			return;
		}

		screen_width_ = screen_width;
		screen_height_ = screen_height;

		// Create screen-space shockwave shader
		shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/shockwave.frag");

		// Create UBO for shockwave data
		glGenBuffers(1, &ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_);

		// Allocate space for: count (16 bytes for alignment) + array of shockwave data
		size_t ubo_size = 16 + kMaxShockwaves * sizeof(ShockwaveGPUData);
		glBufferData(GL_UNIFORM_BUFFER, ubo_size, nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Create intermediate FBO for effect rendering
		glGenFramebuffers(1, &fbo_);
		glGenTextures(1, &output_texture_);

		glBindTexture(GL_TEXTURE_2D, output_texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screen_width_, screen_height_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture_, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			logger::ERROR("Shockwave FBO incomplete!");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		initialized_ = true;
		logger::INFO("ShockwaveManager initialized");
	}

	void ShockwaveManager::Resize(int width, int height) {
		if (width == screen_width_ && height == screen_height_) {
			return;
		}

		screen_width_ = width;
		screen_height_ = height;

		if (output_texture_ != 0) {
			glBindTexture(GL_TEXTURE_2D, output_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	void ShockwaveManager::EnsureInitialized() {
		if (!initialized_) {
			logger::WARNING("ShockwaveManager::EnsureInitialized called but manager not initialized!");
		}
	}

	bool ShockwaveManager::AddShockwave(
		const glm::vec3& center,
		const glm::vec3& normal,
		float            max_radius,
		float            duration,
		float            intensity,
		float            ring_width,
		const glm::vec3& color
	) {
		if (shockwaves_.size() >= kMaxShockwaves) {
			// Remove the oldest shockwave to make room
			auto oldest = std::min_element(
				shockwaves_.begin(),
				shockwaves_.end(),
				[](const Shockwave& a, const Shockwave& b) { return a.GetNormalizedAge() > b.GetNormalizedAge(); }
			);
			if (oldest != shockwaves_.end()) {
				shockwaves_.erase(oldest);
			} else {
				logger::WARNING("ShockwaveManager at capacity, cannot add new shockwave");
				return false;
			}
		}

		Shockwave wave{};
		wave.center = center;
		wave.normal = normal;
		wave.max_radius = max_radius;
		wave.current_radius = 0.0f;
		wave.duration = duration;
		wave.elapsed_time = 0.0f;
		wave.intensity = intensity;
		wave.ring_width = ring_width;
		wave.color = color;

		shockwaves_.push_back(wave);
		return true;
	}

	void ShockwaveManager::Update(float delta_time) {
		// Update all shockwaves
		for (auto& wave : shockwaves_) {
			wave.elapsed_time += delta_time;

			// Calculate current radius using eased expansion
			// Use sqrt for a more dramatic initial burst that slows over time
			float t = wave.GetNormalizedAge();
			// Ease-out cubic for dramatic expansion
			float eased_t = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
			wave.current_radius = wave.max_radius * eased_t;
		}

		// Remove expired shockwaves
		shockwaves_.erase(
			std::remove_if(shockwaves_.begin(), shockwaves_.end(), [](const Shockwave& w) { return w.IsExpired(); }),
			shockwaves_.end()
		);
	}

	void ShockwaveManager::UpdateShaderData() {
		if (!initialized_ || shockwaves_.empty()) {
			return;
		}

		// Prepare GPU data
		std::vector<ShockwaveGPUData> gpu_data;
		gpu_data.reserve(shockwaves_.size());

		for (const auto& wave : shockwaves_) {
			ShockwaveGPUData data{};
			data.center_radius = glm::vec4(wave.center, wave.current_radius);
			data.normal_unused = glm::vec4(wave.normal, 0.0f);
			data.params = glm::vec4(
				wave.GetEffectiveIntensity() * global_intensity_,
				wave.ring_width,
				wave.max_radius,
				wave.GetNormalizedAge()
			);
			data.color_unused = glm::vec4(wave.color, 0.0f);
			gpu_data.push_back(data);
		}

		// Upload to UBO
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_);

		// Write count (padded to 16 bytes for std140)
		int count = static_cast<int>(gpu_data.size());
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(int), &count);

		// Write shockwave array
		if (!gpu_data.empty()) {
			glBufferSubData(
				GL_UNIFORM_BUFFER,
				16, // After count padding
				gpu_data.size() * sizeof(ShockwaveGPUData),
				gpu_data.data()
			);
		}

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void ShockwaveManager::BindUBO(GLuint binding_point) const {
		if (ubo_ != 0) {
			glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo_);
		}
	}

	void ShockwaveManager::ApplyScreenSpaceEffect(
		GLuint           source_texture,
		GLuint           depth_texture,
		const glm::mat4& view_matrix,
		const glm::mat4& proj_matrix,
		const glm::vec3& camera_pos,
		GLuint           quad_vao
	) {
		EnsureInitialized();

		if (shockwaves_.empty() || !shader_) {
			return;
		}

		// Update UBO data first
		UpdateShaderData();

		// Set viewport to match the screen size for this post-processing stage
		glViewport(0, 0, screen_width_, screen_height_);

		shader_->use();

		// Set uniforms (count is in UBO, so we don't need numShockwaves uniform)
		shader_->setInt("sceneTexture", 0);
		shader_->setInt("depthTexture", 1);
		shader_->setVec2("screenSize", glm::vec2(screen_width_, screen_height_));
		shader_->setVec3("cameraPos", camera_pos);
		shader_->setMat4("viewMatrix", view_matrix);
		shader_->setMat4("projMatrix", proj_matrix);

		// Calculate near/far planes from projection matrix for depth linearization
		// For a perspective projection: proj[2][2] = -(far+near)/(far-near), proj[3][2] = -2*far*near/(far-near)
		float A = proj_matrix[2][2];
		float B = proj_matrix[3][2];
		float near_plane = B / (A - 1.0f);
		float far_plane = B / (A + 1.0f);
		shader_->setFloat("nearPlane", near_plane);
		shader_->setFloat("farPlane", far_plane);

		// Bind shockwave UBO to binding point 3 (after Lighting=0, VisualEffects=1, other=2)
		BindUBO(3);
		glUniformBlockBinding(shader_->ID, glGetUniformBlockIndex(shader_->ID, "Shockwaves"), 3);

		// Bind scene texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, source_texture);

		// Bind depth texture
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depth_texture);

		// Render full-screen quad
		glBindVertexArray(quad_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glActiveTexture(GL_TEXTURE0);
	}

} // namespace Boidsish
