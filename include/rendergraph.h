#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

// 1. The Interface
class Base {
public:
	virtual ~Base() = default;
	virtual void doSomething() = 0;
};

// 2. C++20 Concept (Optional but recommended for compile-time safety)
template <typename T>
concept Registerable = std::derived_from<T, Base> && requires {
	{ T::name } -> std::convertible_to<std::string>;
};

// 3. The Factory
class Factory {
public:
	using CreatorFunc = std::function<std::unique_ptr<Base>()>;

	// Meyer's Singleton: Thread-safe and guarantees correct initialization order
	static Factory& get() {
		static Factory instance;
		return instance;
	}

	bool registerClass(const std::string& name, CreatorFunc func) {
		registry_[name] = std::move(func);
		return true;
	}

	std::unique_ptr<Base> create(const std::string& name) {
		if (auto it = registry_.find(name); it != registry_.end()) {
			return it->second();
		}
		return nullptr;
	}

private:
	std::unordered_map<std::string, CreatorFunc> registry_;
};

// 4. The Self-Registering Class
class MyDerived: public Base {
public:
	inline static const std::string name = "MyDerived";

	void doSomething() override { std::cout << "MyDerived doing something.\n"; }

private:
	// This single line replaces all older CRTP and macro boilerplate.
	// The compiler guarantees this is evaluated before main().
	inline static const bool registered = Factory::get().registerClass(name,
	                                                                   [] { return std::make_unique<MyDerived>(); });
};

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Handles for transient resources
struct TextureHandle {
	uint32_t id;
};

struct RenderGraphBuilder {
	TextureHandle create_texture(std::string name) { return {0}; }

	void read(TextureHandle tex) {}

	void write(TextureHandle tex) {}
};

struct RenderContext {
	void draw_mesh() { /* Raw graphics API commands */ }
};

class RenderGraph {
public:
	template <typename PassData, typename SetupFunc, typename ExecFunc>
	void add_pass(std::string name, SetupFunc setup, ExecFunc execute) {
		RenderGraphBuilder builder;
		PassData           data = setup(builder);

		// Store execution logic for later traversal
		m_passes.push_back([data, execute](RenderContext& ctx) { execute(data, ctx); });
	}

	void execute(RenderContext& ctx) {
		// 1. Compile (Topological sort & automatically place resource barriers here)
		// 2. Execute active passes sequentially
		for (auto& pass : m_passes) {
			pass(ctx);
		}
	}

private:
	std::vector<std::function<void(RenderContext&)>> m_passes;
};

// --- Usage Example ---
void render_frame() {
	RenderGraph   graph;
	RenderContext context;

	// Pass 1: Draw depth maps or scene geometry
	struct GBufferData {
		TextureHandle color;
	};

	graph.add_pass<GBufferData>(
		"GBufferPass",
		[](RenderGraphBuilder& builder) -> GBufferData {
			TextureHandle rt = builder.create_texture("AlbedoRT");
			builder.write(rt);
			return {rt};
		},
		[](const GBufferData& data, RenderContext& ctx) {
			ctx.draw_mesh(); // Record geometry commands
		});

	// Pass 2: Process or apply effects using Pass 1 outputs
	struct LightingData {
		TextureHandle inputColor;
	};

	graph.add_pass<LightingData>(
		"LightingPass",
		[](RenderGraphBuilder& builder) -> LightingData {
			TextureHandle rt = {0}; // Hook up reference to AlbedoRT
			builder.read(rt);
			return {rt};
		},
		[](const LightingData& data, RenderContext& ctx) {
			// Under the hood, the graph automatically placed a barrier here:
			// Render Target Layout -> Shader Resource View Layout
			ctx.draw_mesh(); // Screen-space lighting pass
		});

	graph.execute(context);
}

#include <algorithm>
#include <execution>
#include <mutex>
#include <thread>
#include <vector>

struct RenderCommand {
	uint32_t meshId;
	uint32_t materialId;
	float    depth;
};

void processRenderGraphMultithreaded(const std::vector<RenderCommand>& unsortedCommands) {
	// 1. Parallel transform or culling
	std::vector<RenderCommand> visibleCommands = unsortedCommands;

	// Process culling/transform calculations in parallel using C++17/20 parallel algorithms
	std::for_each(std::execution::par, visibleCommands.begin(), visibleCommands.end(), [](RenderCommand& cmd) {
		// Perform view-frustum culling or matrix multiplication math here
		cmd.depth += 1.0f; // Example operation
	});

	// 2. Sort by material/depth for state optimization
	std::sort(std::execution::par,
	          visibleCommands.begin(),
	          visibleCommands.end(),
	          [](const RenderCommand& a, const RenderCommand& b) { return a.materialId < b.materialId; });

	// 3. Thread-safe execution to submit to backend
	static std::mutex           renderMutex;
	std::lock_guard<std::mutex> lock(renderMutex);

	// Backend API submission happens here
	for (const auto& cmd : visibleCommands) {
		// Submit command to API
	}
}

/*

GLFWwindow* window = glfwCreateWindow(1024, 1024, "Legit Vulkan renderer", nullptr, nullptr);

const char* glfwExtensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
uint32_t glfwExtensionCount = 2;

legit::WindowDesc windowDesc = {};
windowDesc.hInstance = GetModuleHandle(NULL);
windowDesc.hWnd = glfwGetWin32Window(window);

auto core = std::make_unique<legit::Core>(glfwExtensions, glfwExtensionCount, &windowDesc, true);

while (glfwWindowShouldClose(window))
{
  glfwPollEvents();
  if (!inFlightQueue)
  {
    core->ClearCaches();
    inFlightQueue = std::unique_ptr<legit::InFlightQueue>(new legit::InFlightQueue(core.get(), windowDesc, 2,
vk::PresentModeKHR::eMailbox));
  }

  try
  {
    auto frameInfo = inFlightQueue->BeginFrame();
    {
      core->GetRenderGraph()->AddPass(legit::RenderGraph::RenderPassDesc()
        .SetColorAttachments({ { frameInfo.swapchainImageViewProxyId, vk::AttachmentLoadOp::eDontCare } })
        .SetRenderAreaExtent(this->viewportExtent)
        .SetRecordFunc([this](legit::RenderGraph::RenderPassContext passContext)
      {
        auto pipeineInfo = core->GetPipelineCache()->BindGraphicsPipeline(passContext.GetCommandBuffer(),
passContext.GetRenderPass()->GetHandle(), legit::DepthSettings::Disabled(), { legit::BlendSettings::Opaque() },
legit::VertexDeclaration(), vk::PrimitiveTopology::eTriangleFan, shader.program.get());
        passContext.GetCommandBuffer().draw(4, 1, 0, 0);
      }));
    }
    inFlightQueue->EndFrame();
  }
  catch (vk::OutOfDateKHRError err)
  {
    core->WaitIdle();
    inFlightQueue.reset();
  }
}
core->WaitIdle();

glfwDestroyWindow(window);

*/

/*
// The base interface all your passes will inherit from
class IRenderPass {
public:
	virtual ~IRenderPass() = default;

	// Phase 1: Declare what virtual textures this pass reads and writes.
	// Executed every frame during graph building.
	virtual void Setup(RenderGraphBuilder& builder) = 0;

	// Phase 2: Do the actual OpenGL work.
	// Only executed if the graph determines this pass's output is needed.
	virtual void Execute(const RenderGraphResources& resources) = 0;
};

// A concrete implementation
class VolumetricFilterPass: public IRenderPass {
public:
	void Setup(RenderGraphBuilder& builder) override {
		// We request a texture created by a previous pass
		m_inputHandle = builder.Read("RawClouds");

		// We declare what we are creating
		m_outputHandle = builder.Write("FilteredClouds", TextureDesc{...});
	}

	void Execute(const RenderGraphResources& resources) override {
		GLuint inputGL = resources.GetTexture(m_inputHandle);
		GLuint outputGL = resources.GetTexture(m_outputHandle);

		// Bind shaders, dispatch compute using the GLuints...
	}

private:
	RenderGraphHandle m_inputHandle;
	RenderGraphHandle m_outputHandle;
};

// 1. Setup Phase (Build the DAG)
for (auto* pass : m_registeredPasses) {
	pass->Setup(graphBuilder);
}

// 2. Compile Phase (Topological sort, resource aliasing, culling)
graphBuilder.Compile("FinalBackbufferImage"); // Trace backwards from the final requested output

// 3. Execute Phase (Run the un-culled passes)
graphBuilder.Execute();

class VolumetricSVGFPass: public IRenderPass {
public:
	void Setup(RenderGraphBuilder& builder) override {
		// Always read the input
		m_inputHandle = builder.Read("RawClouds");

		if (EngineConfig::Get().EnableVolumetricSVGF) {
			// Normal operation: We will run the filter.
			// Declare the new texture we will write to.
			m_outputHandle = builder.Write("FilteredClouds", TextureDesc{...});
		} else {
			// Pass-Through: We will NOT run the filter.
			// Tell the graph that anyone asking for "FilteredClouds"
			// should just be given "RawClouds" instead.
			builder.Forward("FilteredClouds", m_inputHandle);
		}
	}

	void Execute(const RenderGraphResources& resources) override {
		// If the pass was forwarded or culled by the graph, Execute() is never called.
		// We only do GL work if the filter is actually running.
		GLuint inputGL = resources.GetTexture(m_inputHandle);
		GLuint outputGL = resources.GetTexture(m_outputHandle);

		// Dispatch SVGF compute...
	}
};

*/