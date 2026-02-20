#include "post_processing/TemporalAccumulator.h"

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		TemporalAccumulator::TemporalAccumulator() {}

		TemporalAccumulator::~TemporalAccumulator() {
			Cleanup();
		}

		void TemporalAccumulator::Initialize(int width, int height, GLenum internalFormat) {
			_width = width;
			_height = height;
			_internalFormat = internalFormat;

			_accumulationShader = std::make_unique<ComputeShader>("shaders/effects/temporal_accumulation.comp");
			CreateTextures();
		}

		void TemporalAccumulator::Cleanup() {
			if (_historyTextures[0])
				glDeleteTextures(2, _historyTextures);
			_historyTextures[0] = 0;
			_historyTextures[1] = 0;
		}

		void TemporalAccumulator::CreateTextures() {
			Cleanup();
			glGenTextures(2, _historyTextures);
			for (int i = 0; i < 2; i++) {
				glBindTexture(GL_TEXTURE_2D, _historyTextures[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, _internalFormat, _width, _height, 0, GL_RED, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void TemporalAccumulator::Resize(int width, int height) {
			_width = width;
			_height = height;
			CreateTextures();
		}

		GLuint TemporalAccumulator::Accumulate(GLuint currentFrame, GLuint velocityTexture, GLuint depthTexture) {
			if (!_accumulationShader || !_accumulationShader->isValid())
				return currentFrame;

			int nextIndex = 1 - _currentIndex;

			_accumulationShader->use();
			_accumulationShader->setFloat("uAlpha", 0.9f);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, currentFrame);
			_accumulationShader->setInt("uCurrentFrame", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _historyTextures[_currentIndex]);
			_accumulationShader->setInt("uHistoryFrame", 1);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, velocityTexture);
			_accumulationShader->setInt("uVelocity", 2);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			_accumulationShader->setInt("uDepth", 3);

			// Format must match shader. Using GL_R16F in C++ but rgba16f in shader might be okay if we only use .r
			// Actually, let's fix the shader to use r16f if we use GL_R16F.
			glBindImageTexture(0, _historyTextures[nextIndex], 0, GL_FALSE, 0, GL_WRITE_ONLY, _internalFormat);

			glDispatchCompute((_width + 7) / 8, (_height + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			_currentIndex = nextIndex;
			return _historyTextures[_currentIndex];
		}

	} // namespace PostProcessing
} // namespace Boidsish
