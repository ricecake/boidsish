#pragma once

#include <memory>
#include <vector>

#include "IPostProcessingEffect.h"
#include <GL/glew.h>

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class PostProcessingManager {
		public:
			PostProcessingManager(int width, int height, GLuint quad_vao);
			~PostProcessingManager();

			void   Initialize();
			void   AddEffect(std::shared_ptr<IPostProcessingEffect> effect);
			void   SetToneMappingEffect(std::shared_ptr<IPostProcessingEffect> effect);

			void SetSharedDepthTexture(GLuint texture);

			void BeginApply(GLuint sourceTexture, GLuint sourceFbo, GLuint depthTexture);

			void AttachDepthToCurrentFBO();
			void DetachDepthFromPingPongFBOs();

			void ApplyEarlyEffects(
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos,
				float            time
			);
			void ApplyLateEffects(
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos,
				float            time
			);

			GLuint GetFinalTexture() const { return current_texture_; }

			GLuint GetCurrentFBO() const { return current_fbo_; }

			// Deprecated - use BeginApply/ApplyEarlyEffects/ApplyLateEffects instead
			GLuint ApplyEffects(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos,
				float            time
			);
			void Resize(int width, int height);

			std::vector<std::shared_ptr<IPostProcessingEffect>>& GetPreToneMappingEffects() {
				return pre_tone_mapping_effects_;
			}

			std::shared_ptr<IPostProcessingEffect> GetToneMappingEffect() { return tone_mapping_effect_; }

		private:
			void InitializeFBOs();
			void ApplyEffectInternal(
				std::shared_ptr<IPostProcessingEffect> effect,
				const glm::mat4&                       viewMatrix,
				const glm::mat4&                       projectionMatrix,
				const glm::vec3&                       cameraPos,
				float                                  time
			);

			int                                                 width_, height_;
			std::vector<std::shared_ptr<IPostProcessingEffect>> pre_tone_mapping_effects_;
			std::shared_ptr<IPostProcessingEffect>              tone_mapping_effect_;
			GLuint                                              quad_vao_;

			GLuint pingpong_fbo_[2];
			GLuint pingpong_texture_[2];
			GLuint shared_depth_texture_ = 0;

			// State for multi-stage application
			GLuint current_texture_ = 0;
			GLuint current_fbo_ = 0;
			GLuint depth_texture_ = 0;
			int    fbo_index_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
