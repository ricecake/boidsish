#include "post_processing/effects/DepthOfFieldEffect.h"
#include "shader.h"
#include <GLFW/glfw3.h>

namespace Boidsish {
	namespace PostProcessing {

		DepthOfFieldEffect::DepthOfFieldEffect()
			: RenderGraphPostProcessingEffect("Depth of Field"),
			  autofocusEnabled_(true), manualFocusDistance_(10.0f), focalPointOffset_(0.0f),
			  minFocusDistance_(0.1f), maxFocusDistance_(1000.0f),
			  focusScale_(5.0f), blurSize_(6.0f) {
			is_enabled_ = false;

			cocPass_ = graph_.AddPass<CoCPass>();
			blurPass_ = graph_.AddPass<DoFBlurPass>();
		}

		DepthOfFieldEffect::~DepthOfFieldEffect() = default;

		void DepthOfFieldEffect::Initialize(int width, int height) {
			cocPass_->SetParameters(&params_);
			blurPass_->SetParameters(&params_);

			RenderGraphPostProcessingEffect::Initialize(width, height);
		}

		void DepthOfFieldEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			params_.autofocusEnabled = autofocusEnabled_;
			params_.manualFocusDistance = manualFocusDistance_;
			params_.focalPointOffset = focalPointOffset_;
			params_.minFocusDistance = minFocusDistance_;
			params_.maxFocusDistance = maxFocusDistance_;
			params_.focusScale = focusScale_;
			params_.blurSize = blurSize_;
			params_.projectionMatrix = projectionMatrix;
			params_.width = width_;
			params_.height = height_;

			RenderGraphPostProcessingEffect::Apply(sourceTexture, depthTexture, velocityTexture, normalTexture, albedoTexture, viewMatrix, projectionMatrix, cameraPos);
		}

	} // namespace PostProcessing
} // namespace Boidsish
