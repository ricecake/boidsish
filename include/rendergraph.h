#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <GL/glew.h>
#include "render_context.h"

namespace Boidsish {

	// --- Resource Descriptions ---

	enum class TextureFormat {
		RGBA16F,
		R16F,
		RG16F,
		RGBA8,
	};

	struct ResourceDesc {
		int           width = 0;
		int           height = 0;
		TextureFormat format = TextureFormat::RGBA16F;
	};

	struct TextureHandle {
		uint32_t    id = 0;
		std::string name;

		bool IsValid() const { return id != 0 || !name.empty(); }
		auto operator<=>(const TextureHandle&) const = default;
	};

	// --- Render Graph Resources (what passes see during Execute) ---

	class RenderGraphResources {
	public:
		virtual ~RenderGraphResources() = default;
		virtual GLuint GetTexture(const std::string& name) const = 0;
		virtual GLuint GetFBO(const std::string& name) const = 0;
	};

	// --- Builder (what passes see during Setup) ---

	class RenderGraphBuilder {
	public:
		virtual ~RenderGraphBuilder() = default;
		virtual TextureHandle Read(std::string name) = 0;
		virtual TextureHandle Write(std::string name, ResourceDesc desc = {}) = 0;
		virtual void          SetSideEffect() = 0;
	};

	// --- Render Pass Interface ---

	class IRenderPass {
	public:
		virtual ~IRenderPass() = default;
		virtual void Setup(RenderGraphBuilder& builder) = 0;
		virtual void Execute(const RenderContext& ctx, const RenderGraphResources& resources) = 0;
		virtual bool IsEnabled() const { return true; }
	};

	// --- Render Graph ---

	class RenderGraph {
	public:
		RenderGraph();
		~RenderGraph();

		template <typename T, typename... Args>
		T* AddPass(Args&&... args) {
			m_passes.push_back(std::make_unique<T>(std::forward<Args>(args)...));
			m_needs_compile = true;
			return static_cast<T*>(m_passes.back().get());
		}

		void Compile(int default_width, int default_height);
		void Execute(const RenderContext& ctx);

		const std::vector<IRenderPass*>& GetExecutionOrder() const { return m_execution_order; }

		void ImportTexture(const std::string& name, GLuint texture_id);
		void SetOutput(const std::string& resource_name, GLuint fbo);
		void SetOutputFBO(GLuint fbo);

		void OnBegin(std::function<void(const RenderContext&)> func) { m_begin_hooks.push_back(std::move(func)); }
		void OnEnd(std::function<void(const RenderContext&)> func) { m_end_hooks.push_back(std::move(func)); }

		bool NeedsCompile() const { return m_needs_compile; }

		const std::vector<std::unique_ptr<IRenderPass>>& GetPasses() const { return m_passes; }

	public:
		struct PhysicalResource {
			GLuint        texture = 0;
			GLuint        fbo = 0;
			ResourceDesc  desc;
			bool          imported = false;
		};

	private:
		struct PassData {
			IRenderPass*             pass = nullptr;
			std::vector<std::string> reads;
			std::vector<std::string> writes;
			bool                     has_side_effect = false;
		};

		std::vector<std::unique_ptr<IRenderPass>> m_passes;
		std::vector<PassData>                     m_pass_data;
		std::vector<IRenderPass*>                 m_execution_order;

		std::unordered_map<std::string, PhysicalResource> m_resources;
		std::unordered_map<std::string, ResourceDesc>    m_resource_descs;
		std::string m_output_resource;
		GLuint m_output_fbo = 0;
		bool   m_needs_compile = true;

		std::vector<std::function<void(const RenderContext&)>> m_begin_hooks;
		std::vector<std::function<void(const RenderContext&)>> m_end_hooks;

		void AllocateResources(int default_width, int default_height);
		void BuildExecutionOrder();
		void DestroyOwnedResources();
	};

} // namespace Boidsish
