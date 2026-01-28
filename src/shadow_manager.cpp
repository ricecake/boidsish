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
			kMaxShadowMaps,
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
		for (int i = 0; i < kMaxShadowMaps; ++i) {
			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_array_, 0, i);
			glClearDepth(1.0);
			glClear(GL_DEPTH_BUFFER_BIT);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Create shadow UBO for light-space matrices
		// Layout:
		// mat4 lightSpaceMatrices[kMaxShadowMaps]
		// vec4 cascadeSplits
		// int numShadowLights
		glGenBuffers(1, &shadow_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, shadow_ubo_);
		size_t ubo_size = sizeof(glm::mat4) * kMaxShadowMaps + 16 + 16; // matrices + splits + count + padding

		// Initialize UBO with zeros to prevent garbage data
		std::vector<char> zero_data(ubo_size, 0);
		glBufferData(GL_UNIFORM_BUFFER, ubo_size, zero_data.data(), GL_DYNAMIC_DRAW);

		glBindBufferBase(GL_UNIFORM_BUFFER, 2, shadow_ubo_); // Binding point 2 for shadows
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		initialized_ = true;
		logger::INFO("ShadowManager initialized with {} shadow map slots", kMaxShadowMaps);
	}

	void ShadowManager::BeginShadowPass(
		int              map_index,
		const Light&     light,
		const glm::vec3& scene_center,
		float            scene_radius,
		int              cascade_index,
		const glm::mat4& view,
		float            fov,
		float            aspect
	) {
		if (!initialized_ || map_index >= kMaxShadowMaps) {
			return;
		}

		// Store current viewport
		glGetIntegerv(GL_VIEWPORT, prev_viewport_);

		// Calculate light-space matrix
		glm::vec3 light_dir = glm::normalize(light.direction);
		if (light.type != DIRECTIONAL_LIGHT && light.type != SPOT_LIGHT) {
			light_dir = glm::normalize(scene_center - light.position);
		}

		glm::mat4 light_view;
		glm::mat4 light_projection;

		if (light.type == DIRECTIONAL_LIGHT && cascade_index >= 0) {
			// CSM for directional light
			float near_split = (cascade_index == 0) ? 0.1f : cascade_splits_[cascade_index - 1];
			float far_split = cascade_splits_[cascade_index];

			glm::mat4 cascade_proj = glm::perspective(
				glm::radians(fov),
				aspect,
				near_split,
				far_split
			);

			auto corners = GetFrustumCornersWorldSpace(cascade_proj, view);

			glm::vec3 center = glm::vec3(0, 0, 0);
			for (const auto& v : corners) {
				center += glm::vec3(v);
			}
			center /= (float)corners.size();

			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
			if (std::abs(glm::dot(light_dir, up)) > 0.99f) {
				up = glm::vec3(0.0f, 0.0f, 1.0f);
			}
			light_view = glm::lookAt(center - light_dir * scene_radius, center, up);

			float minX = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float minY = std::numeric_limits<float>::max();
			float maxY = std::numeric_limits<float>::lowest();
			for (const auto& v : corners) {
				const auto trf = light_view * v;
				minX = std::min(minX, trf.x);
				maxX = std::max(maxX, trf.x);
				minY = std::min(minY, trf.y);
				maxY = std::max(maxY, trf.y);
			}

			// Expand the ortho bounds slightly to include casters that are just outside
			// the frustum but could cast shadows into it.
			float padding = 50.0f;
			minX -= padding;
			maxX += padding;
			minY -= padding;
			maxY += padding;

			// Use a fixed large depth range to include all potential casters
			// The light is positioned at Z=0 and looking towards -Z.
			// center is at Z=-scene_radius.
			light_projection = glm::ortho(minX, maxX, minY, maxY, -scene_radius * 2.0f, scene_radius * 2.0f);
		} else {
			// Standard shadow map
			glm::vec3 look_at_pos;
			glm::vec3 light_pos = light.position;

			if (light.type == DIRECTIONAL_LIGHT) {
				light_pos = scene_center - light_dir * scene_radius;
				look_at_pos = scene_center;
			} else if (light.type == SPOT_LIGHT) {
				look_at_pos = light_pos + light_dir;
			} else {
				look_at_pos = scene_center;
			}

			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
			if (std::abs(glm::dot(light_dir, up)) > 0.99f) {
				up = glm::vec3(0.0f, 0.0f, 1.0f);
			}

			light_view = glm::lookAt(light_pos, look_at_pos, up);

			float ortho_size = scene_radius;
			// Use the same large symmetric depth range for standard shadow maps
			light_projection = glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, -scene_radius * 2.0f, scene_radius * 2.0f);
		}

		light_space_matrices_[map_index] = light_projection * light_view;

		// Set up framebuffer for this shadow map layer
		glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo_);
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_array_, 0, map_index);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			logger::ERROR("Shadow FBO incomplete for map {}: {}", map_index, status);
		}

		glViewport(0, 0, kShadowMapSize, kShadowMapSize);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Enable front-face culling to reduce shadow acne
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		// Set up shadow shader
		shadow_shader_->use();
		shadow_shader_->setMat4("lightSpaceMatrix", light_space_matrices_[map_index]);
	}

	void ShadowManager::EndShadowPass() {
		// Restore culling state
		glCullFace(GL_BACK);

		// Restore previous viewport
		glViewport(prev_viewport_[0], prev_viewport_[1], prev_viewport_[2], prev_viewport_[3]);

		// Unbind framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	const glm::mat4& ShadowManager::GetLightSpaceMatrix(int map_index) const {
		if (map_index >= 0 && map_index < kMaxShadowMaps) {
			return light_space_matrices_[map_index];
		}
		static glm::mat4 identity(1.0f);
		return identity;
	}

	Frustum ShadowManager::GetShadowFrustum(int map_index) const {
		Frustum          frustum;
		const glm::mat4& vp = GetLightSpaceMatrix(map_index);

		// Left plane
		frustum.planes[0].normal.x = vp[0][3] + vp[0][0];
		frustum.planes[0].normal.y = vp[1][3] + vp[1][0];
		frustum.planes[0].normal.z = vp[2][3] + vp[2][0];
		frustum.planes[0].distance = vp[3][3] + vp[3][0];

		// Right plane
		frustum.planes[1].normal.x = vp[0][3] - vp[0][0];
		frustum.planes[1].normal.y = vp[1][3] - vp[1][0];
		frustum.planes[1].normal.z = vp[2][3] - vp[2][0];
		frustum.planes[1].distance = vp[3][3] - vp[3][0];

		// Bottom plane
		frustum.planes[2].normal.x = vp[0][3] + vp[0][1];
		frustum.planes[2].normal.y = vp[1][3] + vp[1][1];
		frustum.planes[2].normal.z = vp[2][3] + vp[2][1];
		frustum.planes[2].distance = vp[3][3] + vp[3][1];

		// Top plane
		frustum.planes[3].normal.x = vp[0][3] - vp[0][1];
		frustum.planes[3].normal.y = vp[1][3] - vp[1][1];
		frustum.planes[3].normal.z = vp[2][3] - vp[2][1];
		frustum.planes[3].distance = vp[3][3] - vp[3][1];

		// Near plane
		frustum.planes[4].normal.x = vp[0][3] + vp[0][2];
		frustum.planes[4].normal.y = vp[1][3] + vp[1][2];
		frustum.planes[4].normal.z = vp[2][3] + vp[2][2];
		frustum.planes[4].distance = vp[3][3] + vp[3][2];

		// Far plane
		frustum.planes[5].normal.x = vp[0][3] - vp[0][2];
		frustum.planes[5].normal.y = vp[1][3] - vp[1][2];
		frustum.planes[5].normal.z = vp[2][3] - vp[2][2];
		frustum.planes[5].distance = vp[3][3] - vp[3][2];

		// Normalize the planes
		for (int i = 0; i < 6; ++i) {
			float length = glm::length(frustum.planes[i].normal);
			frustum.planes[i].normal /= length;
			frustum.planes[i].distance /= length;
		}

		return frustum;
	}

	void ShadowManager::BindForRendering(Shader& shader, int texture_unit) {
		glActiveTexture(GL_TEXTURE0 + texture_unit);
		glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_array_);
		shader.use();
		shader.setInt("shadowMaps", texture_unit);
	}

	void ShadowManager::UpdateShadowUBO(const std::vector<Light*>& shadow_lights) {
		// Active shadow maps might be more than shadow lights due to CSM
		// But for the UBO, we just want to upload all used matrices

		glBindBuffer(GL_UNIFORM_BUFFER, shadow_ubo_);

		// Upload all light-space matrices
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4) * kMaxShadowMaps, light_space_matrices_.data());

		// Upload cascade splits
		size_t splits_offset = sizeof(glm::mat4) * kMaxShadowMaps;
		glBufferSubData(GL_UNIFORM_BUFFER, splits_offset, sizeof(float) * kMaxCascades, cascade_splits_.data());

		// Upload shadow count (at offset after all matrices and splits)
		size_t count_offset = splits_offset + 16; // align to 16 bytes
		active_shadow_count_ = kMaxShadowMaps; // Just indicate we have slots
		glBufferSubData(GL_UNIFORM_BUFFER, count_offset, sizeof(int), &active_shadow_count_);

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	std::vector<glm::vec4> ShadowManager::GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
		const auto inv = glm::inverse(proj * view);

		std::vector<glm::vec4> frustumCorners;
		for (unsigned int x = 0; x < 2; ++x) {
			for (unsigned int y = 0; y < 2; ++y) {
				for (unsigned int z = 0; z < 2; ++z) {
					const glm::vec4 pt = inv * glm::vec4(
						2.0f * x - 1.0f,
						2.0f * y - 1.0f,
						2.0f * z - 1.0f,
						1.0f);
					frustumCorners.push_back(pt / pt.w);
				}
			}
		}

		return frustumCorners;
	}

} // namespace Boidsish
