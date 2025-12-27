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
			GLuint ApplyEffects(GLuint sourceTexture);
			void   Resize(int width, int height);
			void   Render();

			std::vector<std::shared_ptr<IPostProcessingEffect>>& GetEffects() { return effects_; }

		private:
			void InitializeFBOs();

			int                                                 width_, height_;
			std::vector<std::shared_ptr<IPostProcessingEffect>> effects_;
			GLuint                                              quad_vao_;

			GLuint pingpong_fbo_[2];
			GLuint pingpong_texture_[2];
		};

	} // namespace PostProcessing
} // namespace Boidsish
