#include "rendergraph.h"

#include <algorithm>
#include <map>
#include <queue>
#include <vector>

namespace Boidsish {
namespace {

	GLenum ToGLFormat(TextureFormat fmt) {
		switch (fmt) {
			case TextureFormat::RGBA16F: return GL_RGBA16F;
			case TextureFormat::R16F:    return GL_R16F;
			case TextureFormat::RG16F:   return GL_RG16F;
			case TextureFormat::RGBA8:   return GL_RGBA8;
		}
		return GL_RGBA16F;
	}

	GLenum ToGLPixelFormat(TextureFormat fmt) {
		switch (fmt) {
			case TextureFormat::RGBA16F:
			case TextureFormat::RGBA8:   return GL_RGBA;
			case TextureFormat::R16F:    return GL_RED;
			case TextureFormat::RG16F:   return GL_RG;
		}
		return GL_RGBA;
	}

	class GraphBuilder : public RenderGraphBuilder {
	public:
		struct PassInfo {
			std::vector<std::string>                        reads;
			std::vector<std::pair<std::string, ResourceDesc>> writes;
			bool                                            has_side_effect = false;
		};

		PassInfo& Current() { return m_current; }
		void Reset() { m_current = {}; }

		TextureHandle Read(std::string name) override {
			m_current.reads.push_back(name);
			return TextureHandle{0, name};
		}

		TextureHandle Write(std::string name, ResourceDesc desc) override {
			m_current.writes.emplace_back(name, desc);
			return TextureHandle{0, name};
		}

		void SetSideEffect() override {
			m_current.has_side_effect = true;
		}

	private:
		PassInfo m_current;
	};

	class GraphResources : public RenderGraphResources {
	public:
		GraphResources(const std::unordered_map<std::string, RenderGraph::PhysicalResource>& resources)
			: m_resources(resources) {}

		GLuint GetTexture(const std::string& name) const override {
			if (auto it = m_resources.find(name); it != m_resources.end()) {
				return it->second.texture;
			}
			return 0;
		}

		GLuint GetFBO(const std::string& name) const override {
			if (auto it = m_resources.find(name); it != m_resources.end()) {
				return it->second.fbo;
			}
			return 0;
		}

	private:
		const std::unordered_map<std::string, RenderGraph::PhysicalResource>& m_resources;
	};

} // anonymous namespace

	// --- RenderGraph Implementation ---

	RenderGraph::RenderGraph() = default;

	RenderGraph::~RenderGraph() {
		DestroyOwnedResources();
	}

	void RenderGraph::ImportTexture(const std::string& name, GLuint texture_id) {
		auto& res = m_resources[name];
		res.texture = texture_id;
		res.imported = true;
		res.fbo = 0;
	}

	void RenderGraph::SetOutput(const std::string& resource_name, GLuint fbo) {
		m_output_resource = resource_name;
		m_output_fbo = fbo;
	}

	void RenderGraph::SetOutputFBO(GLuint fbo) {
		m_output_fbo = fbo;
	}

	void RenderGraph::Compile(int default_width, int default_height) {
		m_pass_data.clear();
		GraphBuilder builder;

		for (auto& pass : m_passes) {
			builder.Reset();
			pass->Setup(builder);

			PassData pd;
			pd.pass = pass.get();
			pd.has_side_effect = builder.Current().has_side_effect;
			for (auto& r : builder.Current().reads) {
				pd.reads.push_back(r);
			}
			for (auto& [name, desc] : builder.Current().writes) {
				pd.writes.push_back(name);
				m_resource_descs[name] = desc;
			}
			m_pass_data.push_back(std::move(pd));
		}

		BuildExecutionOrder();

		if (default_width > 0 && default_height > 0) {
			AllocateResources(default_width, default_height);
		}

		m_needs_compile = false;
	}

	void RenderGraph::AllocateResources(int default_width, int default_height) {
		for (auto& [name, raw_desc] : m_resource_descs) {
			if (!m_output_resource.empty() && name == m_output_resource) continue;
			ResourceDesc desc = raw_desc;
			if (desc.width == 0) desc.width = default_width;
			if (desc.height == 0) desc.height = default_height;
			if (m_resources.count(name) && m_resources[name].imported) continue;

			auto& res = m_resources[name];
			bool needs_create = (res.texture == 0);
			bool needs_resize = (!needs_create && (res.desc.width != desc.width || res.desc.height != desc.height || res.desc.format != desc.format));

			if (needs_create || needs_resize) {
				if (res.texture) {
					glDeleteTextures(1, &res.texture);
					res.texture = 0;
				}
				if (res.fbo) {
					glDeleteFramebuffers(1, &res.fbo);
					res.fbo = 0;
				}

				glGenTextures(1, &res.texture);
				glBindTexture(GL_TEXTURE_2D, res.texture);
				glTexImage2D(GL_TEXTURE_2D, 0, ToGLFormat(desc.format),
					desc.width, desc.height, 0,
					ToGLPixelFormat(desc.format), GL_FLOAT, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				glGenFramebuffers(1, &res.fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, res.fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, res.texture, 0);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				res.desc = desc;
				res.imported = false;
			}
		}
	}

	void RenderGraph::BuildExecutionOrder() {
		m_execution_order.clear();

		// Build producer map: resource name -> index of pass that writes it
		std::unordered_map<std::string, size_t> producers;
		for (size_t i = 0; i < m_pass_data.size(); ++i) {
			for (auto& w : m_pass_data[i].writes) {
				producers[w] = i;
			}
		}

		// Build adjacency + in-degree for topological sort
		std::vector<std::vector<size_t>> adj(m_pass_data.size());
		std::vector<int> in_degree(m_pass_data.size(), 0);

		for (size_t i = 0; i < m_pass_data.size(); ++i) {
			for (auto& r : m_pass_data[i].reads) {
				auto it = producers.find(r);
				if (it != producers.end() && it->second != i) {
					adj[it->second].push_back(i);
					in_degree[i]++;
				}
			}
		}

		// Kahn's algorithm, seeded in declaration order for determinism
		std::queue<size_t> queue;
		for (size_t i = 0; i < m_pass_data.size(); ++i) {
			if (in_degree[i] == 0) {
				queue.push(i);
			}
		}

		while (!queue.empty()) {
			size_t idx = queue.front();
			queue.pop();

			if (m_pass_data[idx].pass->IsEnabled()) {
				m_execution_order.push_back(m_pass_data[idx].pass);
			}

			for (size_t next : adj[idx]) {
				if (--in_degree[next] == 0) {
					queue.push(next);
				}
			}
		}
	}

	void RenderGraph::Execute(const RenderContext& ctx) {
		bool has_gpu_resources = std::any_of(m_resources.begin(), m_resources.end(),
			[](const auto& pair) { return pair.second.texture != 0; });

		for (auto& hook : m_begin_hooks) {
			hook(ctx);
		}

		GraphResources resources(m_resources);

		for (size_t i = 0; i < m_execution_order.size(); ++i) {
			IRenderPass* pass = m_execution_order[i];

			if (has_gpu_resources) {
				for (auto& pd : m_pass_data) {
					if (pd.pass != pass) continue;

					if (!pd.writes.empty()) {
						const std::string& target = pd.writes[0];
						if (!m_output_resource.empty() && target == m_output_resource) {
							glBindFramebuffer(GL_FRAMEBUFFER, m_output_fbo);
						} else {
							auto it = m_resources.find(target);
							if (it != m_resources.end() && it->second.fbo != 0) {
								glBindFramebuffer(GL_FRAMEBUFFER, it->second.fbo);
							} else {
								glBindFramebuffer(GL_FRAMEBUFFER, m_output_fbo);
							}
						}
						glClear(GL_COLOR_BUFFER_BIT);
					}
					break;
				}

				glDisable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);
			}

			pass->Execute(ctx, resources);
		}

		if (has_gpu_resources) {
			glBindFramebuffer(GL_FRAMEBUFFER, m_output_fbo);
			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);
		}

		for (auto& hook : m_end_hooks) {
			hook(ctx);
		}
	}

	void RenderGraph::DestroyOwnedResources() {
		for (auto& [name, res] : m_resources) {
			if (res.imported) continue;
			if (res.texture) glDeleteTextures(1, &res.texture);
			if (res.fbo) glDeleteFramebuffers(1, &res.fbo);
		}
		m_resources.clear();
	}

} // namespace Boidsish
