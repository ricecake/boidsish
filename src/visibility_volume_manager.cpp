#include "visibility_volume_manager.h"

#include "graphics.h"
#include "frustum.h"
#include "logger.h"
#include "shader.h"
#include "shadow_manager.h"

#include <GL/glew.h>

namespace Boidsish {

	VisibilityVolumeManager::VisibilityVolumeManager() {}

	VisibilityVolumeManager::~VisibilityVolumeManager() {
		if (volume_texture_ != 0) {
			glDeleteTextures(1, &volume_texture_);
		}
	}

	void VisibilityVolumeManager::Initialize() {
		if (initialized_)
			return;

		volume_compute_shader_ = std::make_unique<ComputeShader>("shaders/visibility_volume.comp");
		if (!volume_compute_shader_->isValid()) {
			logger::ERROR("Failed to compile visibility volume compute shader");
			return;
		}

		CreateTexture();
		initialized_ = true;
	}

	void VisibilityVolumeManager::CreateTexture() {
		glGenTextures(1, &volume_texture_);
		glBindTexture(GL_TEXTURE_3D, volume_texture_);
		glTexImage3D(
			GL_TEXTURE_3D,
			0,
			GL_R16UI,
			kVolumeSize,
			kVolumeSize,
			kVolumeSize,
			0,
			GL_RED_INTEGER,
			GL_UNSIGNED_SHORT,
			nullptr
		);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
		uint32_t border_color[] = {0, 0, 0, 0};
		glTexParameterIuiv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, border_color);
	}

	void VisibilityVolumeManager::Update(
		const Camera& camera,
		const glm::mat4& view,
		const glm::mat4& projection,
		ShadowManager* shadow_manager,
		GLuint hiz_texture,
		const glm::mat4& hiz_prev_vp,
		float time
	) {
		if (!initialized_ || !volume_compute_shader_ || !volume_compute_shader_->isValid())
			return;

		// Snap volume to voxel grid to reduce flickering/shimmering
		glm::vec3 cameraPos(camera.x, camera.y, camera.z);
		glm::vec3 new_origin =
			glm::floor((cameraPos - glm::vec3(kVolumeSize * kVoxelSize * 0.5f)) / kVoxelSize) * kVoxelSize;

		if (new_origin != volume_origin_) {
			// If volume shifted, clear it to prevent history artifacts from wrong world positions
			uint32_t zero = 0;
			glClearTexImage(volume_texture_, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, &zero);
			volume_origin_ = new_origin;
		}

		volume_compute_shader_->use();
		volume_compute_shader_->setVec3("u_volumeOrigin", volume_origin_);
		volume_compute_shader_->setFloat("u_voxelSize", kVoxelSize);
		volume_compute_shader_->setInt("u_volumeSize", kVolumeSize);
		volume_compute_shader_->setFloat("u_time", time);

		Frustum cam_frustum = Frustum::FromViewProjection(view, projection);
		for (int i = 0; i < 6; ++i) {
			volume_compute_shader_->setVec4(
				"u_cameraFrustumPlanes[" + std::to_string(i) + "]",
				glm::vec4(cam_frustum.planes[i].normal, cam_frustum.planes[i].distance)
			);
		}

	volume_compute_shader_->setFloat("u_near", 0.1f);
	// Use a fixed far plane for the volume to match standard engine expectations
	volume_compute_shader_->setFloat("u_far", 1000.0f);

		if (shadow_manager) {
			for (int i = 0; i < 4; ++i) {
				volume_compute_shader_->setMat4(
					"u_shadowMatrices[" + std::to_string(i) + "]",
					shadow_manager->GetLightSpaceMatrix(i)
				);
			}
		}

		volume_compute_shader_->setMat4("u_prevViewProj", hiz_prev_vp);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, hiz_texture);
		volume_compute_shader_->setInt("u_hizTexture", 0);

		glBindImageTexture(0, volume_texture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R16UI);

		glDispatchCompute(kVolumeSize / 4, kVolumeSize / 4, kVolumeSize / 4);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	}

} // namespace Boidsish
