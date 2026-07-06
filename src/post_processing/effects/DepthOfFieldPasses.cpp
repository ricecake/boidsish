#include "post_processing/effects/DepthOfFieldPasses.h"
#include "post_processing/RenderGraphPostProcessingEffect.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		CoCPass::CoCPass(RenderGraphPostProcessingEffect* effect) : effect_(effect) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/dof_coc.frag");
		}

		void CoCPass::Setup(RenderGraphBuilder& builder) {
			builder.Read("InDepth");
			builder.Write("CoCTexture");
		}

		void CoCPass::Execute(const RenderContext& ctx, const RenderGraphResources& resources) {
			if (!params_) return;

			glBindFramebuffer(GL_FRAMEBUFFER, effect_->GetIntermediateFBO("CoCTexture"));
			glClear(GL_COLOR_BUFFER_BIT);

			shader_->use();
			shader_->setVec2("RESOLUTION", glm::vec2(params_->width, params_->height));
			shader_->setBool("uAutofocus", params_->autofocusEnabled);
			shader_->setFloat("uManualFocusDistance", params_->manualFocusDistance);
			shader_->setVec2("uFocalPointOffset", params_->focalPointOffset);
			shader_->setFloat("uMinFocusDistance", params_->minFocusDistance);
			shader_->setFloat("uMaxFocusDistance", params_->maxFocusDistance);
			shader_->setFloat("uFocusScale", params_->focusScale);
			shader_->setMat4("uInvProjection", glm::inverse(params_->projectionMatrix));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, resources.GetTexture(TextureHandle{.name = "InDepth"}));
			shader_->setInt("depthTexture", 0);

			// Render to CoCTexture - assuming RenderGraph handles FBO binding
			// Wait, the current RenderGraph implementation DOES NOT handle FBO binding or texture allocation yet!
			// I need to add that to RenderGraph::Execute or the wrapper.

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		DoFBlurPass::DoFBlurPass(RenderGraphPostProcessingEffect* effect) : effect_(effect) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/dof_blur.frag");
		}

		void DoFBlurPass::Setup(RenderGraphBuilder& builder) {
			builder.Read("InColor");
			builder.Read("CoCTexture");
			builder.Read("InDepth");
			builder.Write("OutColor");
		}

		void DoFBlurPass::Execute(const RenderContext& ctx, const RenderGraphResources& resources) {
			if (!params_) return;

			shader_->use();
			shader_->setVec2("RESOLUTION", glm::vec2(params_->width, params_->height));
			shader_->setFloat("uBlurSize", params_->blurSize);
			shader_->setMat4("uInvProjection", glm::inverse(params_->projectionMatrix));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, resources.GetTexture(TextureHandle{.name = "InColor"}));
			shader_->setInt("screenTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, resources.GetTexture(TextureHandle{.name = "CoCTexture"}));
			shader_->setInt("cocTexture", 1);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, resources.GetTexture(TextureHandle{.name = "InDepth"}));
			shader_->setInt("depthTexture", 2);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

	} // namespace PostProcessing
} // namespace Boidsish
