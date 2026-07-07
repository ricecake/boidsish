#include "post_processing/RenderGraphPostProcessingEffect.h"

#include "rendergraph.h"
#include <map>
#include <vector>

namespace Boidsish {

	class BetterRenderGraphBuilder : public RenderGraphBuilder {
	public:
		BetterRenderGraphBuilder(std::unordered_map<IRenderPass*, PostProcessing::RenderGraphPostProcessingEffect::PassRequirements>& requirements) :
			m_requirements(requirements), m_current_pass(nullptr) {}
		void SetCurrentPass(IRenderPass* pass) { m_current_pass = pass; }
		TextureHandle Read(std::string name) override {
			m_requirements[m_current_pass].reads.push_back(name);
			return {0, name};
		}
		TextureHandle Write(std::string name, uint32_t width, uint32_t height) override {
			m_requirements[m_current_pass].writes.push_back(name);
			return {0, name};
		}
		void Forward(std::string name, TextureHandle handle) override {
			m_requirements[m_current_pass].forwards[name] = handle.name;
		}
	private:
		std::unordered_map<IRenderPass*, PostProcessing::RenderGraphPostProcessingEffect::PassRequirements>& m_requirements;
		IRenderPass*                              m_current_pass;
	};
}

namespace Boidsish {
	namespace PostProcessing {

		RenderGraphPostProcessingEffect::RenderGraphPostProcessingEffect(std::string name) {
			name_ = std::move(name);
		}

		RenderGraphPostProcessingEffect::~RenderGraphPostProcessingEffect() {
			for (auto const& [name, res] : intermediate_resources_) {
				if (res.id) glDeleteTextures(1, &res.id);
				if (res.fbo) glDeleteFramebuffers(1, &res.fbo);
			}
		}

		void RenderGraphPostProcessingEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			EnsureResources();
		}

		void RenderGraphPostProcessingEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			EnsureResources();
		}

		void RenderGraphPostProcessingEffect::RefreshRequirements() {
			m_pass_requirements.clear();
			BetterRenderGraphBuilder builder(m_pass_requirements);

			for (auto& pass : graph_.GetPasses()) {
				builder.SetCurrentPass(pass.get());
				pass->Setup(builder);
			}
		}

		void RenderGraphPostProcessingEffect::EnsureResources() {
			RefreshRequirements();

			for (auto const& [pass, req] : m_pass_requirements) {
				for (auto const& resName : req.writes) {
					if (resName == "OutColor") continue; // Handled by the caller's FBO

					auto& res = intermediate_resources_[resName];
					if (res.id == 0) {
						glGenTextures(1, &res.id);
						glBindTexture(GL_TEXTURE_2D, res.id);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

						glGenFramebuffers(1, &res.fbo);
						glBindFramebuffer(GL_FRAMEBUFFER, res.fbo);
						glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, res.id, 0);
					} else {
						glBindTexture(GL_TEXTURE_2D, res.id);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
					}
				}
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
			// Populate resources
			resources_.SetTexture("InColor", sourceTexture);
			resources_.SetTexture("InDepth", depthTexture);
			resources_.SetTexture("InVelocity", velocityTexture);
			resources_.SetTexture("InNormal", normalTexture);
			resources_.SetTexture("InAlbedo", albedoTexture);

			for (auto const& [name, res] : intermediate_resources_) {
				resources_.SetTexture(name, res.id);
			}

			// Setup context
			RenderContext ctx;
			ctx.view = viewMatrix;
			ctx.projection = projectionMatrix;
			ctx.view_pos = cameraPos;
			ctx.time = time_;

			if (needs_compile_) {
				graph_.Compile();
				RefreshRequirements();
				needs_compile_ = false;
			}

			GLint originalFbo;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFbo);

			for (auto* pass : graph_.GetActivePasses()) {
				auto it = m_pass_requirements.find(pass);
				if (it != m_pass_requirements.end() && !it->second.writes.empty()) {
					// Post-processing passes usually have only one output.
					const std::string& targetName = it->second.writes[0];
					if (targetName == "OutColor") {
						glBindFramebuffer(GL_FRAMEBUFFER, originalFbo);
					} else if (intermediate_resources_.count(targetName)) {
						glBindFramebuffer(GL_FRAMEBUFFER, intermediate_resources_[targetName].fbo);
						glClear(GL_COLOR_BUFFER_BIT);
					}
				}

				// Post-processing quads should not be depth-tested or write to depth buffer
				glDisable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);

				pass->Execute(ctx, resources_);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, originalFbo);
		}

	} // namespace PostProcessing
} // namespace Boidsish
