#pragma once

#include "shape.h"
#include <GL/glew.h>

namespace Boidsish {

	class Arrow: public Shape {
	public:
		Arrow(
			int   id = 0,
			float x = 0.0f,
			float y = 0.0f,
			float z = 0.0f,
			float cone_height = 0.2f,
			float cone_radius = 0.1f,
			float rod_radius = 0.05f,
			float r = 1.0f,
			float g = 1.0f,
			float b = 1.0f,
			float a = 1.0f
		);
		~Arrow();

		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		// Arrows are not instanced (each has unique geometry)
		std::string GetInstanceKey() const override { return "Arrow:" + std::to_string(GetId()); }

		void SetDirection(const glm::vec3& direction);

	private:
		void InitArrowMesh();
		void DestroyArrowMesh();

		float cone_height_;
		float cone_radius_;
		float rod_radius_;

		unsigned int rod_vao_ = 0;
		unsigned int rod_vbo_ = 0;
		int          rod_vertex_count_ = 0;

		unsigned int cone_vao_ = 0;
		unsigned int cone_vbo_ = 0;
		int          cone_vertex_count_ = 0;
	};

} // namespace Boidsish
