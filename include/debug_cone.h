#pragma once

#include "shape.h"

namespace Boidsish {

	class DebugCone: public Shape {
	public:
		DebugCone(
			int   id = 0,
			float x = 0.0f,
			float y = 0.0f,
			float z = 0.0f,
			float r = 1.0f,
			float g = 1.0f,
			float b = 1.0f,
			float a = 1.0f
		);

		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		std::string GetInstanceKey() const override { return "DebugCone"; }
	};

} // namespace Boidsish
