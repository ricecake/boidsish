#include "rendergraph.h"
#include <map>
#include <queue>
#include <vector>

namespace Boidsish {

	class BetterRenderGraphBuilder: public RenderGraphBuilder {
	public:
		struct PassRequirements {
			std::vector<std::string> reads;
			std::vector<std::string> writes;
			std::map<std::string, std::string> forwards;
		};

		BetterRenderGraphBuilder(std::map<IRenderPass*, PassRequirements>& requirements) :
			m_requirements(requirements), m_current_pass(nullptr) {}

		void SetCurrentPass(IRenderPass* pass) { m_current_pass = pass; }

		TextureHandle Read(std::string name) override {
			m_requirements[m_current_pass].reads.push_back(name);
			TextureHandle h;
			h.id = 0;
			h.name = name;
			return h;
		}

		TextureHandle Write(std::string name, uint32_t width, uint32_t height) override {
			m_requirements[m_current_pass].writes.push_back(name);
			TextureHandle h;
			h.id = 0;
			h.name = name;
			return h;
		}

		void Forward(std::string name, TextureHandle handle) override {
			m_requirements[m_current_pass].forwards[name] = handle.name;
		}

	private:
		std::map<IRenderPass*, PassRequirements>& m_requirements;
		IRenderPass* m_current_pass;
	};

	// --- SimpleRenderGraphResources Implementation ---

	uint32_t SimpleRenderGraphResources::GetTexture(TextureHandle handle) const {
		if (handle.id != 0) return handle.id;
		if (auto it = m_textures.find(handle.name); it != m_textures.end()) {
			return it->second;
		}
		return 0;
	}

	// --- RenderGraph Implementation ---

	RenderGraph::RenderGraph() = default;
	RenderGraph::~RenderGraph() = default;

	void RenderGraph::Setup(RenderGraphBuilder& builder) {
		for (auto& pass : m_passes) {
			pass->Setup(builder);
		}
	}

	void RenderGraph::Compile() {
		std::map<IRenderPass*, BetterRenderGraphBuilder::PassRequirements> requirements;
		BetterRenderGraphBuilder builder(requirements);

		for (auto& pass : m_passes) {
			builder.SetCurrentPass(pass.get());
			pass->Setup(builder);
		}

		// 1. Identify producers for each resource
		std::map<std::string, IRenderPass*> producers;
		// Iterate over m_passes for determinism
		for (auto& pass : m_passes) {
			auto it = requirements.find(pass.get());
			if (it == requirements.end()) continue;

			auto const& req = it->second;
			for (auto const& res : req.writes) {
				producers[res] = pass.get();
			}
			for (auto const& [target, source] : req.forwards) {
				producers[target] = pass.get();
			}
		}

		// 2. Build dependency graph
		std::map<IRenderPass*, std::vector<IRenderPass*>> adjacency;
		std::map<IRenderPass*, int> in_degree_map;

		for (auto& pass : m_passes) {
			in_degree_map[pass.get()] = 0;
		}

		for (auto& pass : m_passes) {
			auto it = requirements.find(pass.get());
			if (it == requirements.end()) continue;

			auto const& req = it->second;
			for (auto const& res : req.reads) {
				if (producers.count(res)) {
					IRenderPass* producer = producers[res];
					if (producer != pass.get()) {
						adjacency[producer].push_back(pass.get());
						in_degree_map[pass.get()]++;
					}
				}
			}
		}

		// 3. Topological Sort (Kahn's algorithm)
		m_active_passes.clear();
		std::queue<IRenderPass*> queue;
		// Deterministic initial queue
		for (auto& pass : m_passes) {
			if (in_degree_map[pass.get()] == 0) {
				queue.push(pass.get());
			}
		}

		while(!queue.empty()) {
			IRenderPass* u = queue.front();
			queue.pop();

			if (u->IsEnabled()) {
				m_active_passes.push_back(u);
			}

			if (adjacency.count(u)) {
				// Adjacency list is deterministic because we populated it by iterating over m_passes
				for (IRenderPass* v : adjacency[u]) {
					if (--in_degree_map[v] == 0) {
						queue.push(v);
					}
				}
			}
		}
	}

	void RenderGraph::Execute(const RenderContext& ctx, const RenderGraphResources& resources) {
		for (auto& hook : m_begin_hooks) {
			hook(ctx);
		}

		for (auto* pass : m_active_passes) {
			pass->Execute(ctx, resources);
		}

		for (auto& hook : m_end_hooks) {
			hook(ctx);
		}
	}

} // namespace Boidsish
