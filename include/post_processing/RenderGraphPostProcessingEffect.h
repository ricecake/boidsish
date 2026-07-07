#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include "rendergraph.h"

namespace Boidsish {
	namespace PostProcessing {

		class RenderGraphPostProcessingEffect : public IPostProcessingEffect {
		public:
			RenderGraphPostProcessingEffect(std::string name);
			~RenderGraphPostProcessingEffect() override;

			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				GLuint           normalTexture,
				GLuint           albedoTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;

			void SetTime(float time) override { time_ = time; }

			RenderGraph& GetGraph() { return graph_; }

		protected:
			RenderGraph graph_;
			int         width_ = 0;
			int         height_ = 0;
			float       time_ = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
