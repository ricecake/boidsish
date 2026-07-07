#include "post_processing/RenderGraphPostProcessingEffect.h"

namespace Boidsish {
	namespace PostProcessing {

		RenderGraphPostProcessingEffect::RenderGraphPostProcessingEffect(std::string name) {
			name_ = std::move(name);
		}

		RenderGraphPostProcessingEffect::~RenderGraphPostProcessingEffect() = default;

		void RenderGraphPostProcessingEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			graph_.Compile(width_, height_);
		}

		void RenderGraphPostProcessingEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			graph_.Compile(width_, height_);
		}

		void RenderGraphPostProcessingEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			GLuint           velocityTexture,
			GLuint           normalTexture,
			GLuint           albedoTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			graph_.ImportTexture("InColor", sourceTexture);
			graph_.ImportTexture("InDepth", depthTexture);
			graph_.ImportTexture("InVelocity", velocityTexture);
			graph_.ImportTexture("InNormal", normalTexture);
			graph_.ImportTexture("InAlbedo", albedoTexture);

			if (graph_.NeedsCompile()) {
				graph_.Compile(width_, height_);
			}

			GLint currentFbo;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFbo);
			graph_.SetOutput("OutColor", static_cast<GLuint>(currentFbo));

			RenderContext ctx;
			ctx.view = viewMatrix;
			ctx.projection = projectionMatrix;
			ctx.view_pos = cameraPos;
			ctx.time = time_;

			graph_.Execute(ctx);
		}

	} // namespace PostProcessing
} // namespace Boidsish
