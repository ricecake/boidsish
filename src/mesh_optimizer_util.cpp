#include "mesh_optimizer_util.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <vector>
#include "logger.h"

namespace Boidsish {

	void MeshOptimizerUtil::Optimize(
		std::vector<Vertex>&       vertices,
		std::vector<unsigned int>& indices,
		const std::string&         model_name
	) {
		if (indices.empty() || vertices.empty())
			return;

		logger::LOG("Optimizing mesh for model: {}", model_name);

		size_t index_count = indices.size();
		size_t vertex_count = vertices.size();

		// 1. Vertex cache optimization - reorders indices to minimize vertex shader invocations
		meshopt_optimizeVertexCache(indices.data(), indices.data(), index_count, vertex_count);

		// 2. Overdraw optimization - reorders indices to minimize overdraw from back-to-front rendering
		meshopt_optimizeOverdraw(
			indices.data(),
			indices.data(),
			index_count,
			&vertices[0].Position.x,
			vertex_count,
			sizeof(Vertex),
			1.05f
		);

		// 3. Vertex fetch optimization - reorders vertices to match index order for better cache locality
		std::vector<Vertex> optimized_vertices(vertex_count);
		meshopt_optimizeVertexFetch(
			optimized_vertices.data(),
			indices.data(),
			index_count,
			vertices.data(),
			vertex_count,
			sizeof(Vertex)
		);
		vertices = std::move(optimized_vertices);
		logger::LOG("Optimization complete for model: {}", model_name);
	}

	void MeshOptimizerUtil::Simplify(
		std::vector<Vertex>&       vertices,
		std::vector<unsigned int>& indices,
		float                      target_error,
		float                      target_ratio,
		unsigned int               flags,
		const std::string&         model_name
	) {
		if (indices.empty() || vertices.empty())
			return;

		size_t index_count = indices.size();
		size_t vertex_count = vertices.size();

		logger::LOG(
			"Simplifying mesh for model: {} (target error: {}, target ratio: {}, flags: {}, original indices: {})",
			model_name,
			target_error,
			target_ratio,
			flags,
			index_count
		);

		size_t target_index_count = static_cast<size_t>(index_count * target_ratio);

		std::vector<unsigned int> simplified_indices(index_count);
		size_t                    simplified_index_count = meshopt_simplify(
            simplified_indices.data(),
            indices.data(),
            index_count,
            &vertices[0].Position.x,
            vertex_count,
            sizeof(Vertex),
            target_index_count,
            target_error,
            flags,
            nullptr
        );

		simplified_indices.resize(simplified_index_count);
		indices = std::move(simplified_indices);

		logger::LOG(
			"Simplification complete for model: {}. Indices: {} -> {} ({}%)",
			model_name,
			index_count,
			simplified_index_count,
			(100.0f * simplified_index_count / index_count)
		);

		// After simplification, some vertices may be unused.
		// Use a remap table to remove unused vertices and reorder them for efficiency.
		std::vector<unsigned int> remap(vertex_count);
		size_t                    optimized_vertex_count = meshopt_optimizeVertexFetchRemap(
            remap.data(),
            indices.data(),
            indices.size(),
            vertex_count
        );

		std::vector<Vertex> optimized_vertices(optimized_vertex_count);
		meshopt_remapVertexBuffer(
			optimized_vertices.data(),
			vertices.data(),
			vertex_count,
			sizeof(Vertex),
			remap.data()
		);
		meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());

		vertices = std::move(optimized_vertices);
	}

} // namespace Boidsish
