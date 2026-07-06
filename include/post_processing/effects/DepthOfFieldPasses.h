#pragma once

#include "rendergraph.h"
#include "shader.h"
#include <glm/glm.hpp>

namespace Boidsish {
	namespace PostProcessing {

		class RenderGraphPostProcessingEffect;

		struct DoFParameters {
			bool      autofocusEnabled;
			float     manualFocusDistance;
			glm::vec2 focalPointOffset;
			float     minFocusDistance;
			float     maxFocusDistance;
			float     focusScale;
			float     blurSize;
			glm::mat4 projectionMatrix;
			int       width;
			int       height;
		};

		class CoCPass : public IRenderPass {
		public:
			CoCPass(RenderGraphPostProcessingEffect* effect);
			void Setup(RenderGraphBuilder& builder) override;
			void Execute(const RenderContext& ctx, const RenderGraphResources& resources) override;

			void SetParameters(const DoFParameters* params) { params_ = params; }

		private:
			std::unique_ptr<Shader>           shader_;
			const DoFParameters*              params_ = nullptr;
			RenderGraphPostProcessingEffect*  effect_;
		};

		class DoFBlurPass : public IRenderPass {
		public:
			DoFBlurPass(RenderGraphPostProcessingEffect* effect);
			void Setup(RenderGraphBuilder& builder) override;
			void Execute(const RenderContext& ctx, const RenderGraphResources& resources) override;

			void SetParameters(const DoFParameters* params) { params_ = params; }

		private:
			std::unique_ptr<Shader>           shader_;
			const DoFParameters*              params_ = nullptr;
			RenderGraphPostProcessingEffect*  effect_;
		};

	} // namespace PostProcessing
} // namespace Boidsish
