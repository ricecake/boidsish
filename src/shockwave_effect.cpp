#include "shockwave_effect.h"

#include <algorithm>

#include "service_locator.h"

#include "logger.h"
#include "profiler.h"
#include "shader.h"
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	ShockwaveManager::ShockwaveManager(ServiceLocator& /*loc*/) {
		shockwaves_.reserve(kMaxShockwaves);
	}

	ShockwaveManager::~ShockwaveManager() {
		if (fbo_ != 0) {
			glDeleteFramebuffers(1, &fbo_);
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
		ubo_ = std::make_unique<PersistentBuffer<ShockwaveUboData>>(GL_UNIFORM_BUFFER, 1, 3);
		memset(ubo_->GetFullBufferPtr(), 0, ubo_->GetTotalSize());

		// Create intermediate FBO for effect rendering
		glGenFramebuffers(1, &fbo_);
		output_texture_ = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, GL_RGBA16F, screen_width_, screen_height_, 1, 1);

		GLuint id = output_texture_->GetId();
		glBindTexture(GL_TEXTURE_2D, id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);

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

		if (output_texture_) {
			output_texture_ = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, GL_RGBA16F, width, height, 1, 1);
			GLuint id = output_texture_->GetId();
			glBindTexture(GL_TEXTURE_2D, id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
		PROJECT_PROFILE_SCOPE("ShockwaveManager::Update");
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
		PROJECT_PROFILE_SCOPE("ShockwaveManager::UpdateShaderData");
		if (!initialized_) {
			return;
		}

		ubo_->AdvanceFrame();
		ShockwaveUboData* data_ptr = ubo_->GetFrameDataPtr();
		int               count = 0;

		for (const auto& wave : shockwaves_) {
			if (count >= kMaxShockwaves)
				break;

			ShockwaveGPUData& data = data_ptr->shockwaves[count];
			data.center_radius = glm::vec4(wave.center, wave.current_radius);
			data.normal_unused = glm::vec4(wave.normal, 0.0f);
			data.params = glm::vec4(
				wave.GetEffectiveIntensity() * global_intensity_,
				wave.ring_width,
				wave.max_radius,
				wave.GetNormalizedAge()
			);
			data.color_unused = glm::vec4(wave.color, 0.0f);
			count++;
		}

		data_ptr->count = count;
	}

	void ShockwaveManager::BindUBO(GLuint binding_point) const {
		if (ubo_) {
			ubo_->BindRange(binding_point);
		}
	}

	bool ShockwaveManager::ApplyScreenSpaceEffect(
		GLuint           source_texture,
		GLuint           depth_texture,
		const glm::mat4& view_matrix,
		const glm::mat4& proj_matrix,
		const glm::vec3& camera_pos,
		GLuint           quad_vao,
		int              target_width,
		int              target_height
	) {
		EnsureInitialized();

		if (shockwaves_.empty() || !shader_) {
			return false;
		}

		// Update UBO data first
		UpdateShaderData();

		int v_width = (target_width > 0) ? target_width : screen_width_;
		int v_height = (target_height > 0) ? target_height : screen_height_;

		// Set viewport to match the screen size for this post-processing stage
		glViewport(0, 0, v_width, v_height);

		shader_->use();

		// Set uniforms (count is in UBO, so we don't need numShockwaves uniform)
		shader_->setInt("sceneTexture", 0);
		shader_->setInt("depthTexture", 1);
		shader_->setVec2("screenSize", glm::vec2(v_width, v_height));
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

		// Bind shockwave UBO to centralized binding point
		BindUBO(Constants::UboBinding::Shockwaves());
		glUniformBlockBinding(
			shader_->ID,
			glGetUniformBlockIndex(shader_->ID, "Shockwaves"),
			Constants::UboBinding::Shockwaves()
		);

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
		return true;
	}

} // namespace Boidsish
