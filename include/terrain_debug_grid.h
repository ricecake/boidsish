#pragma once

#include "shape.h"
#include <GL/glew.h>
#include <mutex>
#include <vector>

namespace Boidsish {

	class ITerrainGenerator;

	class TerrainDebugGrid: public Shape {
	public:
		TerrainDebugGrid(int id = 0);
		~TerrainDebugGrid() override;

		void Update(float delta_time) override;
		void UpdateGrid(const glm::vec3& camera_pos, const ITerrainGenerator& generator);

		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override { return glm::mat4(1.0f); }

		std::string GetInstanceKey() const override { return "TerrainDebugGrid"; }

	private:
		void InitializeGrid();
		void CleanupGrid();

		struct InstanceData {
			glm::mat4 matrix;
			glm::vec4 color;
		};

		mutable GLuint instance_matrix_vbo_ = 0;
		mutable GLuint instance_color_vbo_ = 0;
		mutable size_t instance_count_ = 0;
		mutable bool   dirty_ = false;

		std::vector<InstanceData> grid_data_;
		mutable std::mutex        data_mutex_;
	};

} // namespace Boidsish
