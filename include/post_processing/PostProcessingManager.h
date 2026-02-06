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
			GLuint ApplyEffects(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos,
				float            time
			);

			// New multi-stage API
			void StartFrame(GLuint sourceTexture, GLuint depthTexture, float time);
			void ApplyPreTransparencyEffects(
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			);
			void ApplyPostTransparencyEffects(
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			);
			GLuint GetCurrentResult() const { return current_texture_; }
			GLuint GetCurrentFBO() const {
				return effect_applied_ ? pingpong_fbo_[1 - fbo_index_] : 0;
			} // This is complex...
			void Resize(int width, int height);

			std::vector<std::shared_ptr<IPostProcessingEffect>>& GetPreToneMappingEffects() {
				return pre_tone_mapping_effects_;
			}

			std::shared_ptr<IPostProcessingEffect> GetToneMappingEffect() { return tone_mapping_effect_; }

		private:
			void InitializeFBOs();

			int                                                 width_, height_;
			std::vector<std::shared_ptr<IPostProcessingEffect>> pre_tone_mapping_effects_;
			std::shared_ptr<IPostProcessingEffect>              tone_mapping_effect_;
			GLuint                                              quad_vao_;

			GLuint pingpong_fbo_[2];
			GLuint pingpong_texture_[2];

			// Current frame state
			GLuint current_texture_ = 0;
			GLuint depth_texture_ = 0;
			float  time_ = 0.0f;
			int    fbo_index_ = 0;
			bool   effect_applied_ = false;
		};

	} // namespace PostProcessing
} // namespace Boidsish
