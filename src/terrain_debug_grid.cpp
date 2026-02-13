#include "terrain_debug_grid.h"

#include "shader.h"
#include "terrain_generator_interface.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	TerrainDebugGrid::TerrainDebugGrid(int id): Shape(id) {
		SetInstanced(false); // We handle our own instancing for performance
	}

	TerrainDebugGrid::~TerrainDebugGrid() {
		CleanupGrid();
	}

	void TerrainDebugGrid::Update(float delta_time) {
		(void)delta_time;
	}

	void TerrainDebugGrid::UpdateGrid(const glm::vec3& camera_pos, const ITerrainGenerator& generator) {
		const int   grid_size = 200;
		const float spacing = 5.0f;
		const int   total_cones = grid_size * grid_size;

		std::lock_guard<std::mutex> lock(data_mutex_);

		if (grid_data_.size() != (size_t)total_cones) {
			grid_data_.resize(total_cones);
		}

		float start_x = std::floor(camera_pos.x / spacing) * spacing - (grid_size / 2) * spacing;
		float start_z = std::floor(camera_pos.z / spacing) * spacing - (grid_size / 2) * spacing;

		glm::vec3 up(0, 1, 0);
		glm::vec3 cone_scale(0.5f, 2.0f, 0.5f);
		glm::vec4 cone_color(1.0f, 1.0f, 0.0f, 1.0f);

		for (int i = 0; i < grid_size; ++i) {
			for (int j = 0; j < grid_size; ++j) {
				float x = start_x + i * spacing;
				float z = start_z + j * spacing;

				auto [height, normal] = generator.GetTerrainPropertiesAtPoint(x, z);

				auto& data = grid_data_[i * grid_size + j];

				glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, height, z));

				if (glm::length(normal) > 0.001f && glm::length(glm::cross(up, normal)) > 0.001f) {
					model = model * glm::mat4_cast(glm::rotation(up, normal));
				}

				model = glm::scale(model, cone_scale);

				data.matrix = model;
				data.color = cone_color;
			}
		}

		instance_count_ = total_cones;
		dirty_ = true;
	}

	void TerrainDebugGrid::render() const {
		if (Shape::shader) {
			render(*Shape::shader, glm::mat4(1.0f));
		}
	}

	void TerrainDebugGrid::render(Shader& shader, const glm::mat4& model_matrix) const {
		(void)model_matrix;

		if (instance_count_ == 0 || cone_vao_ == 0)
			return;

		std::lock_guard<std::mutex> lock(data_mutex_);

		if (instance_matrix_vbo_ == 0) {
			const_cast<TerrainDebugGrid*>(this)->InitializeGrid();
		}

		glBindBuffer(GL_ARRAY_BUFFER, instance_matrix_vbo_);
		if (dirty_) {
			std::vector<glm::mat4> matrices;
			std::vector<glm::vec4> colors;
			matrices.reserve(instance_count_);
			colors.reserve(instance_count_);
			for (const auto& d : grid_data_) {
				matrices.push_back(d.matrix);
				colors.push_back(d.color);
			}

			glBufferData(GL_ARRAY_BUFFER, matrices.size() * sizeof(glm::mat4), matrices.data(), GL_STREAM_DRAW);

			glBindBuffer(GL_ARRAY_BUFFER, instance_color_vbo_);
			glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4), colors.data(), GL_STREAM_DRAW);

			dirty_ = false;
		}

		shader.setBool("is_instanced", true);
		shader.setBool("useInstanceColor", true);
		shader.setBool("isColossal", false);
		shader.setFloat("objectAlpha", 1.0f);

		glBindVertexArray(cone_vao_);

		// Validate EBO binding (crucial if it was lost)
		GLint current_ebo = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &current_ebo);
		if (current_ebo == 0 && cone_ebo_ != 0) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cone_ebo_);
		}

		// Matrix attribute
		glBindBuffer(GL_ARRAY_BUFFER, instance_matrix_vbo_);
		for (int i = 0; i < 4; i++) {
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
			glVertexAttribDivisor(3 + i, 1);
		}

		// Color attribute
		glBindBuffer(GL_ARRAY_BUFFER, instance_color_vbo_);
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
		glVertexAttribDivisor(7, 1);

		glDrawElementsInstanced(GL_TRIANGLES, cone_vertex_count_, GL_UNSIGNED_INT, 0, (GLsizei)instance_count_);

		for (int i = 0; i < 4; i++) {
			glVertexAttribDivisor(3 + i, 0);
			glDisableVertexAttribArray(3 + i);
		}
		glVertexAttribDivisor(7, 0);
		glDisableVertexAttribArray(7);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		shader.setBool("is_instanced", false);
		shader.setBool("useInstanceColor", false);
	}

	void TerrainDebugGrid::InitializeGrid() {
		glGenBuffers(1, &instance_matrix_vbo_);
		glGenBuffers(1, &instance_color_vbo_);
	}

	void TerrainDebugGrid::CleanupGrid() {
		if (instance_matrix_vbo_ != 0) {
			glDeleteBuffers(1, &instance_matrix_vbo_);
			instance_matrix_vbo_ = 0;
		}
		if (instance_color_vbo_ != 0) {
			glDeleteBuffers(1, &instance_color_vbo_);
			instance_color_vbo_ = 0;
		}
	}

} // namespace Boidsish
