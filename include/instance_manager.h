#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "shape.h"
#include <shader.h>

namespace Boidsish {

	class InstanceManager {
	public:
		~InstanceManager();
		void AddInstance(std::shared_ptr<Shape> shape);
		void Render(Shader& shader);

		/**
		 * @brief Perform hardware occlusion queries for all registered instances.
		 */
		void PerformOcclusionQueries(
			const glm::mat4&                    view,
			const glm::mat4&                    projection,
			Shader&                             shader,
			const std::vector<std::shared_ptr<Shape>>& shapes
		);

	private:
		struct QueryInfo {
			unsigned int query_id = 0;
			bool         query_issued = false;
			bool         is_occluded = false;
			int          last_frame_used = -1;
		};

		struct InstanceGroup {
			std::vector<std::shared_ptr<Shape>> shapes;
			unsigned int                        instance_matrix_vbo_ = 0;
			unsigned int                        instance_color_vbo_ = 0;
			size_t                              matrix_capacity_ = 0;
			size_t                              color_capacity_ = 0;
		};

		// Group by instance key (model path for Models, "Dot" for Dots, etc.)
		std::map<std::string, InstanceGroup> m_instance_groups;

		std::map<int, QueryInfo> m_queries;
		int                      m_frame_count = 0;

		void RenderModelGroup(Shader& shader, InstanceGroup& group);
		void RenderDotGroup(Shader& shader, InstanceGroup& group);
	};

} // namespace Boidsish
