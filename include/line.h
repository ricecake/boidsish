#pragma once

#include <string>

#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class Line: public Shape {
	public:
		enum class Style { SOLID = 0, LASER = 1 };

		Line(
			int       id = 0,
			glm::vec3 start = glm::vec3(0.0f),
			glm::vec3 end = glm::vec3(0.0f),
			float     width = 0.1f,
			float     r = 1.0f,
			float     g = 1.0f,
			float     b = 1.0f,
			float     a = 1.0f
		);

		Line(glm::vec3 start, glm::vec3 end, float width = 1.0f);

		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix, const glm::mat4& prev_model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		inline void SetStart(const glm::vec3& start) { SetPosition(start.x, start.y, start.z); }

		inline glm::vec3 GetStart() const { return glm::vec3(GetX(), GetY(), GetZ()); }

		inline void SetEnd(const glm::vec3& end) { end_ = end; }

		inline glm::vec3 GetEnd() const { return end_; }

		inline void SetWidth(float width) { width_ = width; }

		inline float GetWidth() const { return width_; }

		inline void SetStyle(Style style) { style_ = style; }

		inline Style GetStyle() const { return style_; }

		bool IsTransparent() const override { return GetA() < 0.99f || style_ == Style::LASER; }

		static void InitLineMesh();
		static void DestroyLineMesh();

		std::string GetInstanceKey() const override { return "Line:" + std::to_string(GetId()); }

	private:
		glm::vec3 end_;
		float     width_;
		Style     style_ = Style::SOLID;

		static unsigned int line_vao_;
		static unsigned int line_vbo_;
		static int          line_vertex_count_;
	};

} // namespace Boidsish
