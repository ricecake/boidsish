#pragma once

#include <map>
#include <memory>
#include <typeindex>
#include <vector>

#include "shape.h"
#include <shader.h>

namespace Boidsish {

	class InstanceManager {
	public:
		void AddInstance(std::shared_ptr<Shape> shape);
		void Render(Shader& shader);

	private:
		struct InstanceGroup {
			std::vector<std::shared_ptr<Shape>> shapes;
			unsigned int                        instance_vbo_ = 0;
			size_t                              capacity_ = 0;
		};

		std::map<std::type_index, InstanceGroup> m_instance_groups;
	};

} // namespace Boidsish
