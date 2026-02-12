#pragma once

#include "shape.h"

namespace Boidsish {

	class PearShape: public Shape {
	public:
		PearShape(int id = 0);

		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;
		void      GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const override;

		std::string GetInstanceKey() const override { return "PearShape"; }

	private:
		static unsigned int vao_;
		static unsigned int vbo_;
		static unsigned int ebo_;
		static int          vertex_count_;

		void setupMesh() const;
	};

} // namespace Boidsish
