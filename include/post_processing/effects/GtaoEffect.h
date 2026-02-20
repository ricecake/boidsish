#ifndef GTAO_EFFECT_H
#define GTAO_EFFECT_H

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"
#include "post_processing/TemporalAccumulator.h"
#include <glm/glm.hpp>

class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		class GtaoEffect: public IPostProcessingEffect {
		public:
			GtaoEffect();
			~GtaoEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint velocityTexture, GLuint normalTexture, GLuint materialTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;

			void SetIntensity(float intensity) { intensity_ = intensity; }

			float GetIntensity() const { return intensity_; }

			void SetRadius(float radius) { radius_ = radius; }

			float GetRadius() const { return radius_; }

		private:
			void InitializeFBOs();

			std::unique_ptr<ComputeShader> gtao_shader_;
			std::unique_ptr<Shader>        composite_shader_;
			TemporalAccumulator            temporal_accumulator_;

			GLuint ao_texture_ = 0;
			int    width_ = 0, height_ = 0;

			float intensity_ = 0.250f;
			float radius_ = 1.0f;
			int   numSteps_ = 4;
			int   numDirections_ = 2;
		};

	} // namespace PostProcessing
} // namespace Boidsish

#endif // GTAO_EFFECT_H
