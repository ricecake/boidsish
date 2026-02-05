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
		void Render(Shader& shader, bool ignore_occlusion = false);

		/**
		 * @brief Render bounding boxes for occlusion queries.
		 */
		void RenderOcclusionQueries(Shader& shader);

		/**
		 * @brief Clear all registered instances.
		 */
		void ClearInstances();

	private:
		struct InstanceGroup {
			std::vector<std::shared_ptr<Shape>> shapes;
			unsigned int                        instance_matrix_vbo_ = 0;
			unsigned int                        instance_color_vbo_ = 0;
			size_t                              matrix_capacity_ = 0;
			size_t                              color_capacity_ = 0;
		};

		// Group by instance key (model path for Models, "Dot" for Dots, etc.)
		std::map<std::string, InstanceGroup> m_instance_groups;

		void RenderModelGroup(Shader& shader, InstanceGroup& group);
		void RenderDotGroup(Shader& shader, InstanceGroup& group);
	};

} // namespace Boidsish
