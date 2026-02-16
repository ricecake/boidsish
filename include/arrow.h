#pragma once

#include "constants.h"
#include "shape.h"
#include <GL/glew.h>

namespace Boidsish {

	class Arrow: public Shape {
	public:
		Arrow(
			float x = 0.0f,
			float y = 0.0f,
			float z = 0.0f,
			float cone_height = Constants::Class::Shapes::Arrow::DefaultConeHeight(),
			float cone_radius = Constants::Class::Shapes::Arrow::DefaultConeRadius(),
			float rod_radius = Constants::Class::Shapes::Arrow::DefaultRodRadius(),
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
