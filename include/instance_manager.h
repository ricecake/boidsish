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
		InstanceManager();
		~InstanceManager();
		void AddInstance(std::shared_ptr<Shape> shape);
		void Render(Shader& shader);

	private:
		struct InstanceGroup {
			std::vector<std::shared_ptr<Shape>> shapes;
			unsigned int                        instance_matrix_ssbo_ = 0;
			unsigned int                        instance_color_ssbo_ = 0;
			unsigned int                        visible_indices_ssbo_ = 0;
			unsigned int                        indirect_draw_buffer_ = 0;
			size_t                              matrix_capacity_ = 0;
			size_t                              color_capacity_ = 0;
			size_t                              visible_capacity_ = 0;
			glm::mat4*                          matrix_ptr_ = nullptr;
			glm::vec4*                          color_ptr_ = nullptr;
		};

		// Group by instance key (model path for Models, "Dot" for Dots, etc.)
		std::map<std::string, InstanceGroup> m_instance_groups;
		std::unique_ptr<ComputeShader>       m_cull_shader;

		void RenderModelGroup(Shader& shader, InstanceGroup& group);
		void RenderDotGroup(Shader& shader, InstanceGroup& group);
		void InitializeCompute();
	};

} // namespace Boidsish
