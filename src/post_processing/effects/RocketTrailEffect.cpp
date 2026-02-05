#include "post_processing/effects/RocketTrailEffect.h"
#include "constants.h"
#include "shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace Boidsish {
	namespace PostProcessing {

		RocketTrailEffect::RocketTrailEffect() {
			name_ = "RocketTrail";
			grid_size_ = glm::vec3(
				Constants::Class::RocketVoxels::GridSizeX(),
				Constants::Class::RocketVoxels::GridSizeY(),
				Constants::Class::RocketVoxels::GridSizeZ()
			);
			voxel_size_ = Constants::Class::RocketVoxels::VoxelSize();
		}

		RocketTrailEffect::~RocketTrailEffect() {
			ReleaseResources();
		}

		void RocketTrailEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			fragment_shader_ =
				std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/rocket_trail.frag");
			clear_shader_ = std::make_unique<Shader>("shaders/effects/rocket_trail_clear.comp");
			write_shader_ = std::make_unique<Shader>("shaders/effects/rocket_trail_write.comp");

			CreateResources();
		}

		void RocketTrailEffect::CreateResources() {
			if (voxel_texture_ == 0) {
				glGenTextures(1, &voxel_texture_);
				glBindTexture(GL_TEXTURE_3D, voxel_texture_);
				glTexImage3D(
					GL_TEXTURE_3D,
					0,
					GL_R32F,
					Constants::Class::RocketVoxels::GridSizeX(),
					Constants::Class::RocketVoxels::GridSizeY(),
					Constants::Class::RocketVoxels::GridSizeZ(),
					0,
					GL_RED,
					GL_FLOAT,
					nullptr
				);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			}

			if (voxel_ssbo_ == 0) {
				glGenBuffers(1, &voxel_ssbo_);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, voxel_ssbo_);
				// Pre-allocate some space
				glBufferData(GL_SHADER_STORAGE_BUFFER, 10000 * sizeof(RocketVoxel), nullptr, GL_STREAM_DRAW);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
		}

		void RocketTrailEffect::ReleaseResources() {
			if (voxel_texture_) {
				glDeleteTextures(1, &voxel_texture_);
				voxel_texture_ = 0;
			}
			if (voxel_ssbo_) {
				glDeleteBuffers(1, &voxel_ssbo_);
				voxel_ssbo_ = 0;
			}
		}

		void RocketTrailEffect::UpdateVoxels(const RocketVoxelTree& tree, const glm::vec3& cameraPos) {
			if (!clear_shader_ || !clear_shader_->isValid() || !write_shader_ || !write_shader_->isValid())
				return;

			grid_origin_ = cameraPos - grid_size_ * 0.5f;

			// Retrieve ONLY voxels within the grid range from the sparse tree
			std::vector<RocketVoxel> voxels = tree.GetActiveVoxels(grid_origin_, grid_origin_ + grid_size_);
			active_voxel_count_ = static_cast<int>(voxels.size());

			if (active_voxel_count_ > 0) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, voxel_ssbo_);
				glBufferData(
					GL_SHADER_STORAGE_BUFFER,
					voxels.size() * sizeof(RocketVoxel),
					voxels.data(),
					GL_STREAM_DRAW
				);
			}

			// Run compute shaders to update texture
			glBindImageTexture(0, voxel_texture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::UboBinding::RocketVoxels(), voxel_ssbo_);

			// 1. Clear phase
			clear_shader_->use();
			int gx = (Constants::Class::RocketVoxels::GridSizeX() + 7) / 8;
			int gy = (Constants::Class::RocketVoxels::GridSizeY() + 7) / 8;
			int gz = (Constants::Class::RocketVoxels::GridSizeZ() + 7) / 8;
			glDispatchCompute(gx, gy, gz);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Write phase
			if (active_voxel_count_ > 0) {
				write_shader_->use();
				write_shader_->setVec3("gridOrigin", grid_origin_);
				write_shader_->setFloat("voxelSize", voxel_size_);
				write_shader_->setInt("numVoxels", active_voxel_count_);

				int wx = (active_voxel_count_ + 63) / 64;
				glDispatchCompute(wx, 1, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}
		}

		void RocketTrailEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			if (!fragment_shader_ || !fragment_shader_->isValid())
				return;

			fragment_shader_->use();
			fragment_shader_->setInt("sceneTexture", 0);
			fragment_shader_->setInt("depthTexture", 1);
			fragment_shader_->setInt("voxelTexture", Constants::TextureUnit::RocketVoxels());

			fragment_shader_->setVec3("cameraPos", cameraPos);
			fragment_shader_->setMat4("invView", glm::inverse(viewMatrix));
			fragment_shader_->setMat4("invProjection", glm::inverse(projectionMatrix));
			fragment_shader_->setFloat("time", time_);

			fragment_shader_->setVec3("gridOrigin", grid_origin_);
			fragment_shader_->setVec3("gridSize", grid_size_);
			fragment_shader_->setFloat("voxelSize", voxel_size_);
			fragment_shader_->setFloat("maxAge", Constants::Class::RocketVoxels::MaxAge());

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::RocketVoxels());
			glBindTexture(GL_TEXTURE_3D, voxel_texture_);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void RocketTrailEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
