#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "persistent_buffer.h"
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
			std::vector<std::shared_ptr<Shape>>          shapes;
			std::unique_ptr<PersistentBuffer<glm::mat4>> instance_matrix_buffer;
			std::unique_ptr<PersistentBuffer<glm::vec4>> instance_color_buffer;
			std::unique_ptr<PersistentBuffer<glm::mat4>> visible_matrix_buffer;
			std::unique_ptr<PersistentBuffer<glm::vec4>> visible_color_buffer;
			unsigned int                                 indirect_buffer = 0;
			unsigned int                                 atomic_counter_buffer = 0;
			unsigned int                                 handle_ssbo = 0;
		};

		// Group by instance key (model path for Models, "Dot" for Dots, etc.)
		std::map<std::string, InstanceGroup> m_instance_groups;

		std::unique_ptr<ComputeShader> m_culling_shader;
		std::unique_ptr<ComputeShader> m_update_commands_shader;

		void RenderModelGroup(Shader& shader, InstanceGroup& group);
		void RenderDotGroup(Shader& shader, InstanceGroup& group);
		void _InitializeShaders();
	};

} // namespace Boidsish
