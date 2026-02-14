#pragma once

#include <memory>
#include <vector>

#include "IPostProcessingEffect.h"
#include <GL/glew.h>

class ComputeShader;

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
				GLuint           normalTexture,
				GLuint           pbrTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::mat4& prevViewMatrix,
				const glm::mat4& prevProjectionMatrix,
				const glm::vec3& cameraPos,
				float            time,
				float            worldScale,
				uint32_t         frameCount
			);
			void Resize(int width, int height);

			void SetMotionVectorEffect(std::shared_ptr<IPostProcessingEffect> effect);

			std::vector<std::shared_ptr<IPostProcessingEffect>>& GetPreToneMappingEffects() {
				return pre_tone_mapping_effects_;
			}

			std::shared_ptr<IPostProcessingEffect> GetToneMappingEffect() { return tone_mapping_effect_; }

		private:
			void InitializeFBOs();

			int                                                 width_, height_;
			std::shared_ptr<IPostProcessingEffect>              motion_vector_effect_;
			GLuint                                              motion_vector_fbo_{0};
			GLuint                                              motion_vector_texture_{0};
			std::vector<std::shared_ptr<IPostProcessingEffect>> pre_tone_mapping_effects_;
			std::shared_ptr<IPostProcessingEffect>              tone_mapping_effect_;
			GLuint                                              quad_vao_;

			GLuint pingpong_fbo_[2];
			GLuint pingpong_texture_[2];

			GLuint                         hiz_texture_{0};
			std::unique_ptr<ComputeShader> hiz_shader_;
			void                           GenerateHiZ(GLuint depthTexture);
		};

	} // namespace PostProcessing
} // namespace Boidsish
