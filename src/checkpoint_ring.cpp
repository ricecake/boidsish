#include "checkpoint_ring.h"

#include "constants.h"
#include "graphics.h"
#include "shader.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	unsigned int            CheckpointRingShape::quad_vao_ = 0;
	unsigned int            CheckpointRingShape::quad_vbo_ = 0;
	MegabufferAllocation    CheckpointRingShape::quad_alloc_ = {};
	std::shared_ptr<Shader> CheckpointRingShape::checkpoint_shader_ = nullptr;
	ShaderHandle            CheckpointRingShape::checkpoint_shader_handle = ShaderHandle(0);

	CheckpointRingShape::CheckpointRingShape(float radius, CheckpointStyle style):
		Shape(), radius_(radius), style_(style) {
		SetUsePBR(false);
	}

	void CheckpointRingShape::InitQuadMesh(Megabuffer* mb) {
		if (quad_vao_ != 0 || quad_alloc_.valid)
			return;

		struct QuadVertex {
			float x, y, z;
			float u, v;
		};
		QuadVertex raw_vertices[] = {
			{-1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
			{-1.0f, -1.0f, 0.0f, 0.0f, 0.0f},
			{1.0f, 1.0f, 0.0f, 1.0f, 1.0f},
			{1.0f, -1.0f, 0.0f, 1.0f, 0.0f},
		};

		if (mb) {
			std::vector<Vertex> vertices;
			for (int i = 0; i < 4; ++i) {
				Vertex v;
				v.Position = {raw_vertices[i].x, raw_vertices[i].y, raw_vertices[i].z};
				v.Normal = {0, 0, 1};
				v.TexCoords = {raw_vertices[i].u, raw_vertices[i].v};
				v.Color = {1, 1, 1};
				vertices.push_back(v);
			}
			quad_alloc_ = mb->AllocateStatic(4, 0);
			if (quad_alloc_.valid) {
				mb->Upload(quad_alloc_, vertices.data(), 4);
				quad_vao_ = mb->GetVAO();
			}
		} else {
			glGenVertexArrays(1, &quad_vao_);
			glGenBuffers(1, &quad_vbo_);
			glBindVertexArray(quad_vao_);
			glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);

			std::vector<Vertex> vertices;
			for (int i = 0; i < 4; ++i) {
				Vertex v;
				v.Position = {raw_vertices[i].x, raw_vertices[i].y, raw_vertices[i].z};
				v.Normal = {0, 0, 1};
				v.TexCoords = {raw_vertices[i].u, raw_vertices[i].v};
				v.Color = {1, 1, 1};
				vertices.push_back(v);
			}
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
			glEnableVertexAttribArray(8);
			glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));

			glBindVertexArray(0);
		}
	}

	void CheckpointRingShape::DestroyQuadMesh() {
		if (quad_vao_ != 0) {
			glDeleteVertexArrays(1, &quad_vao_);
			glDeleteBuffers(1, &quad_vbo_);
			quad_vao_ = 0;
			quad_vbo_ = 0;
		}
	}

	void CheckpointRingShape::render() const {
		if (!checkpoint_shader_ || quad_vao_ == 0)
			return;
		render(*checkpoint_shader_, GetModelMatrix());
		if (Shape::shader)
			Shape::shader->use();
	}

	void CheckpointRingShape::render(Shader& shader, const glm::mat4& model_matrix) const {
		if (quad_vao_ == 0)
			return;

		shader.use();
		shader.setMat4("model", model_matrix);

		glm::vec3 color(1.0f);
		switch (style_) {
		case CheckpointStyle::GOLD:
			color = Constants::Class::Checkpoint::Colors::Gold();
			break;
		case CheckpointStyle::SILVER:
			color = Constants::Class::Checkpoint::Colors::Silver();
			break;
		case CheckpointStyle::BLACK:
			color = Constants::Class::Checkpoint::Colors::Black();
			break;
		case CheckpointStyle::BLUE:
			color = Constants::Class::Checkpoint::Colors::Blue();
			break;
		case CheckpointStyle::NEON_GREEN:
			color = Constants::Class::Checkpoint::Colors::NeonGreen();
			break;
		default:
			color = glm::vec3(GetR(), GetG(), GetB());
			break;
		}

		shader.setVec3("baseColor", color);
		shader.setInt("style", static_cast<int>(style_));
		shader.setFloat("radius", radius_);
		shader.setBool("use_texture", false);

		glBindVertexArray(quad_vao_);
		if (quad_alloc_.valid) {
			glDrawArrays(GL_TRIANGLE_STRIP, static_cast<GLint>(quad_alloc_.base_vertex), 4);
		} else {
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		glBindVertexArray(0);
	}

	void CheckpointRingShape::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		if (quad_vao_ == 0) return;

		glm::mat4 model_matrix = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(model_matrix[3]);

		RenderPacket packet;
		packet.vao = quad_vao_;
		if (quad_alloc_.valid) {
			packet.base_vertex = quad_alloc_.base_vertex;
		}
		packet.vertex_count = 4;
		packet.draw_mode = GL_TRIANGLE_STRIP;
		packet.shader_id = checkpoint_shader_ ? checkpoint_shader_->ID : 0;

		packet.uniforms.model = model_matrix;

		glm::vec3 color(1.0f);
		switch (style_) {
		case CheckpointStyle::GOLD:
			color = Constants::Class::Checkpoint::Colors::Gold();
			break;
		case CheckpointStyle::SILVER:
			color = Constants::Class::Checkpoint::Colors::Silver();
			break;
		case CheckpointStyle::BLACK:
			color = Constants::Class::Checkpoint::Colors::Black();
			break;
		case CheckpointStyle::BLUE:
			color = Constants::Class::Checkpoint::Colors::Blue();
			break;
		case CheckpointStyle::NEON_GREEN:
			color = Constants::Class::Checkpoint::Colors::NeonGreen();
			break;
		default:
			color = glm::vec3(GetR(), GetG(), GetB());
			break;
		}

		packet.uniforms.color = glm::vec4(color.r, color.g, color.b, GetA());
		packet.uniforms.use_pbr = 0;
		packet.uniforms.use_texture = 0;
		packet.uniforms.is_colossal = IsColossal();

		packet.uniforms.checkpoint_style = static_cast<int>(style_);
		packet.uniforms.checkpoint_radius = radius_;

		packet.casts_shadows = CastsShadows();

		RenderLayer layer = RenderLayer::Transparent;

		packet.shader_handle = checkpoint_shader_handle;
		packet.material_handle = MaterialHandle(0);

		float normalized_depth = context.CalculateNormalizedDepth(world_pos);
		packet.sort_key = CalculateSortKey(layer, packet.shader_handle, packet.material_handle, normalized_depth);

		out_packets.push_back(packet);
	}

	glm::mat4 CheckpointRingShape::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model = model * glm::mat4_cast(GetRotation());
		// Scale by radius * 2 because the ring is at 0.5 in UV space
		model = glm::scale(model, glm::vec3(radius_ * 2.0f));
		return model;
	}

	CheckpointRing::CheckpointRing(int id, float radius, CheckpointStyle style, Callback callback):
		Entity<CheckpointRingShape>(id, radius, style),
		callback_(callback),
		lifespan_(Constants::Class::Checkpoint::DefaultLifespan()) {}

	void CheckpointRing::RegisterEntity(std::shared_ptr<EntityBase> entity) {
		if (!entity)
			return;
		tracked_entities_.push_back({entity->GetId(), entity});
		last_positions_[entity->GetId()] = entity->GetPosition().Toglm();
	}

	void CheckpointRing::UpdateEntity(const EntityHandler& handler, float /*time*/, float delta_time) {
		age_ += delta_time;
		if (age_ > lifespan_) {
			handler.QueueRemoveEntity(id_);
			return;
		}

		glm::vec3 ringPos = GetPosition().Toglm();
		glm::quat ringRot = rigid_body_.GetOrientation();
		glm::vec3 ringNormal = ringRot * glm::vec3(0, 0, 1); // Local Z is forward

		for (auto it = tracked_entities_.begin(); it != tracked_entities_.end();) {
			if (auto entity = it->ptr.lock()) {
				glm::vec3 pos = entity->GetPosition().Toglm();
				int       id = entity->GetId();

				if (last_positions_.find(id) != last_positions_.end()) {
					glm::vec3 lastPos = last_positions_[id];

					// Check if we crossed the plane
					float d1 = glm::dot(lastPos - ringPos, ringNormal);
					float d2 = glm::dot(pos - ringPos, ringNormal);

					if (d1 > 0 && d2 <= 0) { // Passed from front to back
						// Calculate intersection point with plane
						float     t = d1 / (d1 - d2);
						glm::vec3 intersect = lastPos + t * (pos - lastPos);
						float     distFromCenter = glm::distance(intersect, ringPos);

						if (distFromCenter <= shape_->GetRadius()) {
							if (callback_) {
								callback_(distFromCenter, entity);
							}
						}
						// IRRELEVANT now that it has been passed
						handler.QueueRemoveEntity(id_);
						return;
					}
				}
				last_positions_[id] = pos;
				++it;
			} else {
				// Entity destroyed, clean up last_positions_ to prevent memory leak
				last_positions_.erase(it->id);
				it = tracked_entities_.erase(it);
			}
		}
	}

	void CheckpointRing::UpdateShape() {
		Entity<CheckpointRingShape>::UpdateShape();
	}

} // namespace Boidsish
