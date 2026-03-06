#pragma once

#include <memory>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

class ComputeShader;

namespace Boidsish {

	struct Camera;
	struct Frustum;
	class ShadowManager;

	/**
	 * @brief Manages a 3D Visibility Volume used to accelerate culling for GPU-bound resources.
	 *
	 * The visibility volume is a camera-centric 3D texture where each voxel contains a bitmask:
	 * - Bit 0: Inside Main Camera Frustum
	 * - Bit 1: Inside Shadow Cascade 0
	 * - Bit 2: Inside Shadow Cascade 1
	 * - Bit 3: Inside Shadow Cascade 2
	 * - Bit 4: Inside Shadow Cascade 3
	 * - Bit 5: Passed Hi-Z occlusion test (NOT occluded)
	 */
	class VisibilityVolumeManager {
	public:
		static constexpr int kVolumeSize = 128;
		static constexpr float kVoxelSize = 10.0f; // 10 meters per voxel -> 1280m coverage

		VisibilityVolumeManager();
		~VisibilityVolumeManager();

		void Initialize();
		void Update(
			const Camera& camera,
			const glm::mat4& view,
			const glm::mat4& projection,
			ShadowManager* shadow_manager,
			GLuint hiz_texture,
			const glm::mat4& hiz_prev_vp,
			float time,
			float far_plane = 1000.0f
		);

		GLuint GetVolumeTexture() const { return volume_texture_; }
		glm::vec3 GetVolumeOrigin() const { return volume_origin_; }
		float GetVoxelSize() const { return kVoxelSize; }
		int GetVolumeSize() const { return kVolumeSize; }

	private:
		GLuint volume_texture_ = 0;
		std::unique_ptr<ComputeShader> volume_compute_shader_;
		glm::vec3 volume_origin_{0.0f};
		bool initialized_ = false;

		void CreateTexture();
	};

} // namespace Boidsish
