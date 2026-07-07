#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "render_context.h"

namespace Boidsish {

	// --- Factory System ---

	class Base {
	public:
		virtual ~Base() = default;
	};

	class Factory {
	public:
		using CreatorFunc = std::function<std::unique_ptr<Base>()>;

		static Factory& get() {
			static Factory instance;
			return instance;
		}

		template <typename T>
		bool registerClass(CreatorFunc func) {
			registry_[std::type_index(typeid(T))] = std::move(func);
			return true;
		}

		std::unique_ptr<Base> create(std::type_index type) {
			if (auto it = registry_.find(type); it != registry_.end()) {
				return it->second();
			}
			return nullptr;
		}

	private:
		std::unordered_map<std::type_index, CreatorFunc> registry_;
	};

	template <typename T>
	struct Register {
		Register() {
			Factory::get().registerClass<T>([]() { return std::make_unique<T>(); });
		}
	};

	// --- Render Graph Core ---

	struct TextureHandle {
		uint32_t id = 0;
		std::string name; // Include name for easier tracking in this initial version

		bool     IsValid() const { return id != 0 || !name.empty(); }
		auto     operator<=>(const TextureHandle&) const = default;
	};

	struct RenderGraphResources {
		virtual ~RenderGraphResources() = default;
		virtual uint32_t GetTexture(TextureHandle handle) const = 0;
	};

	class SimpleRenderGraphResources : public RenderGraphResources {
	public:
		void     SetTexture(std::string name, uint32_t id) { m_textures[name] = id; }
		uint32_t GetTexture(TextureHandle handle) const override;

	private:
		std::unordered_map<std::string, uint32_t> m_textures;
	};

	class RenderGraphBuilder {
	public:
		virtual ~RenderGraphBuilder() = default;
		virtual TextureHandle Read(std::string name) = 0;
		virtual TextureHandle Write(std::string name, uint32_t width = 0, uint32_t height = 0) = 0;
		virtual void          Forward(std::string name, TextureHandle handle) = 0;
	};

	class IRenderPass: public Base {
	public:
		virtual ~IRenderPass() = default;
		virtual void Setup(RenderGraphBuilder& builder) = 0;
		virtual void Execute(const RenderContext& ctx, const RenderGraphResources& resources) = 0;
		virtual bool IsEnabled() const { return true; }
	};

	class RenderGraph: public IRenderPass {
	public:
		RenderGraph();
		~RenderGraph() override;

		template <typename T, typename... Args>
		T* AddPass(Args&&... args) {
			// Ignore factory for now if arguments are provided
			if constexpr (sizeof...(Args) == 0) {
				auto base_ptr = Factory::get().create(std::type_index(typeid(T)));
				if (base_ptr) {
					m_passes.push_back(std::unique_ptr<IRenderPass>(static_cast<IRenderPass*>(base_ptr.release())));
					return static_cast<T*>(m_passes.back().get());
				}
			}

			m_passes.push_back(std::make_unique<T>(std::forward<Args>(args)...));
			return static_cast<T*>(m_passes.back().get());
		}

		void Setup(RenderGraphBuilder& builder) override;
		void Execute(const RenderContext& ctx, const RenderGraphResources& resources) override;

		void Compile();

		// Hooks
		void OnBegin(std::function<void(const RenderContext&)> func) { m_begin_hooks.push_back(std::move(func)); }
		void OnEnd(std::function<void(const RenderContext&)> func) { m_end_hooks.push_back(std::move(func)); }

		const std::vector<std::unique_ptr<IRenderPass>>& GetPasses() const { return m_passes; }
		const std::vector<IRenderPass*>&                 GetActivePasses() const { return m_active_passes; }

	private:
		std::vector<std::unique_ptr<IRenderPass>> m_passes;
		std::vector<IRenderPass*>                 m_active_passes;

		std::vector<std::function<void(const RenderContext&)>> m_begin_hooks;
		std::vector<std::function<void(const RenderContext&)>> m_end_hooks;
	};

} // namespace Boidsish
