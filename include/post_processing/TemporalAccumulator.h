#ifndef TEMPORAL_ACCUMULATOR_H
#define TEMPORAL_ACCUMULATOR_H

#include <memory>

#include <GL/glew.h>
#include <glm/glm.hpp>

class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		/**
		 * @brief Reusable class for temporal accumulation and reprojection.
		 * Can be used for AO, reflections, clouds, etc.
		 */
		class TemporalAccumulator {
		public:
			TemporalAccumulator();
			~TemporalAccumulator();

			void Initialize(int width, int height, GLenum internalFormat = GL_R16F);
			void Resize(int width, int height);

			/**
			 * @brief Accumulate the current frame's signal with history.
			 * @param currentFrame Signal for the current frame.
			 * @param velocityTexture Velocity buffer (RG16F).
			 * @param depthTexture Depth buffer.
			 * @return The accumulated texture.
			 */
			GLuint Accumulate(GLuint currentFrame, GLuint velocityTexture, GLuint depthTexture);

			GLuint GetResult() const { return _historyTextures[_currentIndex]; }

		private:
			void Cleanup();
			void CreateTextures();

			std::unique_ptr<ComputeShader> _accumulationShader;
			GLuint                         _historyTextures[2] = {0, 0};
			int                            _currentIndex = 0;
			int                            _width = 0, _height = 0;
			GLenum                         _internalFormat = GL_R16F;
		};

	} // namespace PostProcessing
} // namespace Boidsish

#endif // TEMPORAL_ACCUMULATOR_H
