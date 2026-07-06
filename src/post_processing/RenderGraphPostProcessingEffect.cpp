#include "post_processing/RenderGraphPostProcessingEffect.h"

#include "rendergraph.h"
#include <map>
#include <vector>

namespace Boidsish {

	// Internal helper to access requirements
	struct PassRequirements {
		std::vector<std::string>           reads;
		std::vector<std::string>           writes;
		std::map<std::string, std::string> forwards;
	};

	class BetterRenderGraphBuilder : public RenderGraphBuilder {
	public:
		BetterRenderGraphBuilder(std::map<IRenderPass*, PassRequirements>& requirements) :
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
		std::map<IRenderPass*, PassRequirements>& m_requirements;
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

		void RenderGraphPostProcessingEffect::EnsureResources() {
			std::map<IRenderPass*, PassRequirements> requirements;
			BetterRenderGraphBuilder                 builder(requirements);

			for (auto& pass : graph_.GetPasses()) {
				builder.SetCurrentPass(pass.get());
				pass->Setup(builder);
			}

			for (auto const& [pass, req] : requirements) {
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

			// We need to manage FBO switching between passes.
			// This really should be in RenderGraph::Execute, but it doesn't have enough info yet.
			// So for now, we'll manually execute and switch FBOs here if we can.
			// Actually, let's keep it simple and just call Execute, but the passes MUST bind their own FBOs
			// if they know where they are writing.

			// A better way: The RenderGraph should know which FBO to bind for each pass.
			// But since it doesn't, let's just make the passes responsible for now,
			// giving them access to intermediate_resources_ somehow or just letting them use resources_.GetTexture.

			// Actually, I'll update RenderGraph::Execute to take a callback or something,
			// or just wrap each pass execution here.

			graph_.Compile(); // Ensure it's compiled

			GLint originalFbo;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFbo);

			// We need a way to iterate the active passes.
			// Since RenderGraph doesn't expose them easily, I might need to move this logic into RenderGraph
			// or just let passes bind FBOs for now.
			// I'll go with passes binding FBOs for now to keep this change focused.

			graph_.Execute(ctx, resources_);

			glBindFramebuffer(GL_FRAMEBUFFER, originalFbo);
		}

	} // namespace PostProcessing
} // namespace Boidsish
