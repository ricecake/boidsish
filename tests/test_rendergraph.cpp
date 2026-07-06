#include "rendergraph.h"
#include <iostream>
#include <cassert>

using namespace Boidsish;

class MockResources: public RenderGraphResources {
public:
	uint32_t GetTexture(TextureHandle handle) const override { return handle.id; }
};

class TestPassA: public IRenderPass {
public:
	inline static int execute_count = 0;
	void Setup(RenderGraphBuilder& builder) override {
		builder.Write("ResourceA");
	}
	void Execute(const RenderContext&, const RenderGraphResources&) override {
		std::cout << "Executing Pass A\n";
		execute_count++;
	}
};

class TestPassB: public IRenderPass {
public:
	inline static int execute_count = 0;
	void Setup(RenderGraphBuilder& builder) override {
		builder.Read("ResourceA");
		builder.Write("ResourceB");
	}
	void Execute(const RenderContext&, const RenderGraphResources&) override {
		std::cout << "Executing Pass B\n";
		execute_count++;
	}
};

class TestPassDisabled: public IRenderPass {
public:
	inline static int execute_count = 0;
	bool IsEnabled() const override { return false; }
	void Setup(RenderGraphBuilder& builder) override {
		auto h = builder.Read("ResourceB");
		builder.Forward("ResourceC", h);
	}
	void Execute(const RenderContext&, const RenderGraphResources&) override {
		std::cout << "Executing Disabled Pass (should not happen)\n";
		execute_count++;
	}
};

class TestPassC: public IRenderPass {
public:
	inline static int execute_count = 0;
	void Setup(RenderGraphBuilder& builder) override {
		builder.Read("ResourceC");
	}
	void Execute(const RenderContext&, const RenderGraphResources&) override {
		std::cout << "Executing Pass C\n";
		execute_count++;
	}
};

int main() {
	RenderGraph graph;
	graph.AddPass<TestPassA>();
	graph.AddPass<TestPassB>();
	graph.AddPass<TestPassDisabled>();
	graph.AddPass<TestPassC>();

	bool begin_hook_called = false;
	RenderContext dummy_ctx;
	graph.OnBegin([&](const RenderContext&){ begin_hook_called = true; });

	graph.Compile();

	MockResources resources;
	graph.Execute(dummy_ctx, resources);

	assert(begin_hook_called);
	assert(TestPassA::execute_count == 1);
	assert(TestPassB::execute_count == 1);
	assert(TestPassDisabled::execute_count == 0);
	assert(TestPassC::execute_count == 1);

	std::cout << "RenderGraph test passed!\n";

	return 0;
}
