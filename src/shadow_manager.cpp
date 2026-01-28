#include "shadow_manager.h"

#include <iostream>
#include <vector>

#include "light.h"
#include "logger.h"
#include "shader.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	ShadowManager::ShadowManager() {
		light_space_matrices_.fill(glm::mat4(1.0f));
	}

	ShadowManager::~ShadowManager() {
		if (shadow_fbo_ != 0) {
			glDeleteFramebuffers(1, &shadow_fbo_);
		}
		if (shadow_map_array_ != 0) {
			glDeleteTextures(1, &shadow_map_array_);
		}
		if (shadow_ubo_ != 0) {
			glDeleteBuffers(1, &shadow_ubo_);
		}
	}

	void ShadowManager::Initialize() {
		if (initialized_) {
			return;
		}

		// Create shadow depth shader
		shadow_shader_ = std::make_unique<Shader>("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");

		// Create shadow map texture array
		glGenTextures(1, &shadow_map_array_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_array_);
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			GL_DEPTH_COMPONENT24,
			kShadowMapSize,
			kShadowMapSize,
			kMaxShadowLights,
			0,
			GL_DEPTH_COMPONENT,
			GL_FLOAT,
			nullptr
		);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		// Set border color to 1.0 (max depth) so areas outside shadow map are not shadowed
		float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
		glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border_color);
		// Enable depth comparison for shadow sampling
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

		// Create shadow framebuffer
		glGenFramebuffers(1, &shadow_fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo_);
		// We'll attach specific layers when rendering each shadow map
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		// Clear all shadow map layers to max depth (1.0)
		// This ensures unused layers don't cause artifacts
		for (int i = 0; i < kMaxShadowLights; ++i) {
			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_array_, 0, i);
			glClearDepth(1.0);
			glClear(GL_DEPTH_BUFFER_BIT);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Create shadow UBO for light-space matrices
		// Layout: mat4 lightSpaceMatrices[MAX_SHADOW_LIGHTS], int numShadowLights, padding
		glGenBuffers(1, &shadow_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, shadow_ubo_);
		size_t ubo_size = sizeof(glm::mat4) * kMaxShadowLights + 16; // matrices + count + padding

		// Initialize UBO with zeros to prevent garbage data
		std::vector<char> zero_data(ubo_size, 0);
		glBufferData(GL_UNIFORM_BUFFER, ubo_size, zero_data.data(), GL_DYNAMIC_DRAW);

		glBindBufferBase(GL_UNIFORM_BUFFER, 2, shadow_ubo_); // Binding point 2 for shadows
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		initialized_ = true;
		logger::INFO("ShadowManager initialized with {} shadow map slots", kMaxShadowLights);
	}

	void ShadowManager::BeginShadowPass(
		int              light_index,
		const Light&     light,
		const glm::vec3& scene_center,
		float            scene_radius
	) {
		if (!initialized_ || light_index >= kMaxShadowLights) {
			return;
		}

		// Store current viewport
		glGetIntegerv(GL_VIEWPORT, prev_viewport_);

		// Calculate light-space matrix
		glm::vec3 light_dir;
		glm::vec3 look_at_pos;
		glm::vec3 light_pos = light.position;

		// Handle different light types
		if (light.type == DIRECTIONAL_LIGHT) {
			// Directional light: use light direction, position is irrelevant
			// We place the "virtual" light position far enough back to cover the scene
			light_dir = glm::normalize(light.direction);
			light_pos = scene_center - light_dir * scene_radius;
			look_at_pos = scene_center;
		} else if (light.type == SPOT_LIGHT) {
			// Spot light: use light position and direction
			light_dir = glm::normalize(light.direction);
			look_at_pos = light_pos + light_dir;
		} else {
			// Point/Emissive light: use light position, look towards scene center
			light_dir = glm::normalize(scene_center - light_pos);
			look_at_pos = scene_center;
		}

		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		// Handle case where light is directly above/below
		if (std::abs(glm::dot(light_dir, up)) > 0.99f) {
			up = glm::vec3(0.0f, 0.0f, 1.0f);
		}

		glm::mat4 light_view = glm::lookAt(light_pos, look_at_pos, up);

		// Calculate orthographic frustum that encompasses the scene
		// We use the radius directly as the half-extents of the orthographic box
		float ortho_size = scene_radius;
		float light_to_center_dist = glm::length(light_pos - scene_center);

		// Ensure near and far planes cover the entire scene volume
		// We start from the light and go through the scene
		float near_plane = std::max(0.1f, light_to_center_dist - scene_radius * 1.5f);
		float far_plane = light_to_center_dist + scene_radius * 1.5f;

		glm::mat4 light_projection =
			glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, near_plane, far_plane);

		light_space_matrices_[light_index] = light_projection * light_view;

		// Set up framebuffer for this shadow map layer
		glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo_);
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_array_, 0, light_index);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			logger::ERROR("Shadow FBO incomplete for light {}: {}", light_index, status);
		}

		glViewport(0, 0, kShadowMapSize, kShadowMapSize);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Enable front-face culling to reduce shadow acne
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		// Set up shadow shader
		shadow_shader_->use();
		shadow_shader_->setMat4("lightSpaceMatrix", light_space_matrices_[light_index]);
	}

	void ShadowManager::EndShadowPass() {
		// Restore culling state
		glCullFace(GL_BACK);

		// Restore previous viewport
		glViewport(prev_viewport_[0], prev_viewport_[1], prev_viewport_[2], prev_viewport_[3]);

		// Unbind framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	const glm::mat4& ShadowManager::GetLightSpaceMatrix(int light_index) const {
		if (light_index >= 0 && light_index < kMaxShadowLights) {
			return light_space_matrices_[light_index];
		}
		static glm::mat4 identity(1.0f);
		return identity;
	}

	void ShadowManager::BindForRendering(Shader& shader, int texture_unit) {
		glActiveTexture(GL_TEXTURE0 + texture_unit);
		glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_array_);
		shader.use();
		shader.setInt("shadowMaps", texture_unit);
	}

	void ShadowManager::UpdateShadowUBO(const std::vector<Light*>& shadow_lights) {
		active_shadow_count_ = std::min(static_cast<int>(shadow_lights.size()), kMaxShadowLights);

		glBindBuffer(GL_UNIFORM_BUFFER, shadow_ubo_);

		// Upload light-space matrices
		for (int i = 0; i < active_shadow_count_; ++i) {
			glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4), sizeof(glm::mat4), &light_space_matrices_[i]);
		}

		// Upload shadow count (at offset after all matrices)
		size_t count_offset = sizeof(glm::mat4) * kMaxShadowLights;
		glBufferSubData(GL_UNIFORM_BUFFER, count_offset, sizeof(int), &active_shadow_count_);

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

} // namespace Boidsish
