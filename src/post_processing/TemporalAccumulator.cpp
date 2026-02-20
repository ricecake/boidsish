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
			if (_historyDepthTextures[0])
				glDeleteTextures(2, _historyDepthTextures);
			_historyTextures[0] = 0;
			_historyTextures[1] = 0;
			_historyDepthTextures[0] = 0;
			_historyDepthTextures[1] = 0;
		}

		void TemporalAccumulator::CreateTextures() {
			Cleanup();
			glGenTextures(2, _historyTextures);
			glGenTextures(2, _historyDepthTextures);
			for (int i = 0; i < 2; i++) {
				glBindTexture(GL_TEXTURE_2D, _historyTextures[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, _internalFormat, _width, _height, 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				glBindTexture(GL_TEXTURE_2D, _historyDepthTextures[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, _width, _height, 0, GL_RED, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, _historyDepthTextures[_currentIndex]);
			_accumulationShader->setInt("uHistoryDepth", 4);

			glBindImageTexture(0, _historyTextures[nextIndex], 0, GL_FALSE, 0, GL_WRITE_ONLY, _internalFormat);
			glBindImageTexture(1, _historyDepthTextures[nextIndex], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

			glDispatchCompute((_width + 7) / 8, (_height + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			_currentIndex = nextIndex;
			return _historyTextures[_currentIndex];
		}

	} // namespace PostProcessing
} // namespace Boidsish
