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
			void Resize(int width, int height);

			std::vector<std::shared_ptr<IPostProcessingEffect>>& GetPreToneMappingEffects() {
				return pre_tone_mapping_effects_;
			}

			std::shared_ptr<IPostProcessingEffect> GetToneMappingEffect() { return tone_mapping_effect_; }

			template<typename T>
			std::shared_ptr<T> GetEffect(const std::string& name) {
				for (auto& effect : pre_tone_mapping_effects_) {
					if (effect->GetName() == name) {
						return std::dynamic_pointer_cast<T>(effect);
					}
				}
				return nullptr;
			}

		private:
			void InitializeFBOs();

			int                                                 width_, height_;
			std::vector<std::shared_ptr<IPostProcessingEffect>> pre_tone_mapping_effects_;
			std::shared_ptr<IPostProcessingEffect>              tone_mapping_effect_;
			GLuint                                              quad_vao_;

			GLuint pingpong_fbo_[2];
			GLuint pingpong_texture_[2];
		};

	} // namespace PostProcessing
} // namespace Boidsish
