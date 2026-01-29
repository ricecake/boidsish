#include "mesh_explosion_manager.h"

#include <cstring>
#include <iostream>
#include <random>

#include "logger.h"
#include <GL/glew.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	MeshExplosionManager::MeshExplosionManager() {}

	MeshExplosionManager::~MeshExplosionManager() {
		if (ssbo_ != 0) {
			glDeleteBuffers(1, &ssbo_);
		}
		if (vao_ != 0) {
			glDeleteVertexArrays(1, &vao_);
		}
	}

	void MeshExplosionManager::_Initialize() {
		if (initialized_)
			return;

		render_shader_ = std::make_unique<Shader>("shaders/mesh_explosion.vert", "shaders/mesh_explosion.frag");
		compute_shader_ = std::make_unique<ComputeShader>("shaders/mesh_explosion.comp");

		glGenBuffers(1, &ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
		std::vector<MeshExplosionFragment> initial_data(kMaxFragments);
		memset(initial_data.data(), 0, kMaxFragments * sizeof(MeshExplosionFragment));
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			kMaxFragments * sizeof(MeshExplosionFragment),
			initial_data.data(),
			GL_DYNAMIC_DRAW
		);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		glGenVertexArrays(1, &vao_);

		initialized_ = true;
	}

	void MeshExplosionManager::ExplodeShape(std::shared_ptr<Shape> shape, float intensity, const glm::vec3& velocity) {
		_Initialize();
		if (!shape)
			return;

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;
		shape->GetGeometry(vertices, indices);

		if (indices.empty())
			return;

		glm::mat4 model_matrix = shape->GetModelMatrix();
		glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(model_matrix)));
		glm::vec4 color(shape->GetR(), shape->GetG(), shape->GetB(), shape->GetA());

		std::vector<MeshExplosionFragment>    new_fragments;
		std::mt19937                          gen(static_cast<unsigned int>(time_ * 1000));
		std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

		for (size_t i = 0; i < indices.size(); i += 3) {
			const Vertex& v1 = vertices[indices[i]];
			const Vertex& v2 = vertices[indices[i + 1]];
			const Vertex& v3 = vertices[indices[i + 2]];

			glm::vec4 world_v1 = model_matrix * glm::vec4(v1.Position, 1.0f);
			glm::vec4 world_v2 = model_matrix * glm::vec4(v2.Position, 1.0f);
			glm::vec4 world_v3 = model_matrix * glm::vec4(v3.Position, 1.0f);
			glm::vec3 tri_center = (glm::vec3(world_v1) + glm::vec3(world_v2) + glm::vec3(world_v3)) / 3.0f;

			MeshExplosionFragment f;
			f.v0 = world_v1 - glm::vec4(tri_center, 0.0f);
			f.v1 = world_v2 - glm::vec4(tri_center, 0.0f);
			f.v2 = world_v3 - glm::vec4(tri_center, 0.0f);
			f.t01 = glm::vec4(v1.TexCoords.x, v1.TexCoords.y, v2.TexCoords.x, v2.TexCoords.y);
			f.t2_age = glm::vec4(v3.TexCoords.x, v3.TexCoords.y, 0.0f, 2.0f); // age=0, lifetime=2s
			f.normal = glm::vec4(normal_matrix * glm::normalize(v1.Normal + v2.Normal + v3.Normal), 0.0f);
			f.pos = glm::vec4(tri_center, 1.0f);

			// Outward velocity from local center of shape
			glm::vec3 local_tri_center = (v1.Position + v2.Position + v3.Position) / 3.0f;
			glm::vec3 outward = glm::normalize(local_tri_center);
			if (glm::length(local_tri_center) < 0.001f)
				outward = glm::vec3(dist(gen), dist(gen), dist(gen));

			// Transform outward vector by model rotation/scale (using normal matrix is a good approximation for
			// direction)
			outward = glm::normalize(normal_matrix * outward);

			f.vel = glm::vec4(
				velocity + outward * Constants::Class::Explosions::DefaultVelocity() * intensity +
					glm::vec3(dist(gen), dist(gen), dist(gen)) * Constants::Class::Explosions::DefaultRandomVelocity() *
						intensity,
				0.0f
			);
			f.rot = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // identity quat
			f.angVel = glm::vec4(
				glm::normalize(glm::vec3(dist(gen), dist(gen), dist(gen))),
				Constants::Class::Explosions::DefaultRandomVelocity() * intensity
			);
			f.color = color;

			new_fragments.push_back(f);

			if (new_fragments.size() >= kMaxFragments)
				break;
		}

		if (new_fragments.empty())
			return;

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
		if (current_fragment_index_ + new_fragments.size() <= kMaxFragments) {
			glBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				current_fragment_index_ * sizeof(MeshExplosionFragment),
				new_fragments.size() * sizeof(MeshExplosionFragment),
				new_fragments.data()
			);
			current_fragment_index_ = (current_fragment_index_ + new_fragments.size()) % kMaxFragments;
		} else {
			// Circular buffer wrap around
			size_t first_part = kMaxFragments - current_fragment_index_;
			glBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				current_fragment_index_ * sizeof(MeshExplosionFragment),
				first_part * sizeof(MeshExplosionFragment),
				new_fragments.data()
			);
			size_t second_part = new_fragments.size() - first_part;
			glBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				0,
				second_part * sizeof(MeshExplosionFragment),
				new_fragments.data() + first_part
			);
			current_fragment_index_ = second_part;
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void MeshExplosionManager::Update(float delta_time, float time) {
		if (!initialized_)
			return;
		time_ = time;

		compute_shader_->use();
		compute_shader_->setFloat("u_delta_time", delta_time);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
		glDispatchCompute((kMaxFragments / Constants::Class::Explosions::ComputeGroupSize()) + 1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	}

	void MeshExplosionManager::Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos) {
		if (!initialized_)
			return;

		render_shader_->use();
		render_shader_->setMat4("u_view", view);
		render_shader_->setMat4("u_projection", projection);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
		glBindVertexArray(vao_);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 3, kMaxFragments);
		glBindVertexArray(0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	}

} // namespace Boidsish
