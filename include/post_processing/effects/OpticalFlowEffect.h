#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class OpticalFlowEffect: public IPostProcessingEffect {
		public:
			OpticalFlowEffect();
			~OpticalFlowEffect();

			void Apply(GLuint sourceTexture, float delta_time) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

		private:
			void InitializeFBO(int width, int height);
			void CleanupFBO();

			std::unique_ptr<Shader> _shader;
			std::unique_ptr<Shader> _passthroughShader;

			// FBO to store the previous frame's color
			GLuint _previousFrameFBO = 0;
			GLuint _previousFrameTexture = 0;

			// Ping-pong FBOs for optical flow
			GLuint _flowFBOs[2] = {0, 0};
			GLuint _flowTextures[2] = {0, 0};
			int    _currentFlowIndex = 0;

			// FBO for the final output
			GLuint _outputFBO = 0;
			GLuint _outputTexture = 0;

			int _width = 0;
			int _height = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
