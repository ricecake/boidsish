#include "SdfBoidHandler.h"

#include <random>

#include "dot.h"
#include "sdf_volume_manager.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	SdfBoid::SdfBoid(bool predator): Entity<Dot>(), is_predator_(predator) {
		if (predator) {
			SetColor(1.0f, 0.1f, 0.1f);
			SetSize(12.0f);
		} else {
			SetColor(0.2f, 0.6f, 1.0f);
			SetSize(8.0f);
		}
	}

	void SdfBoid::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		glm::vec3 pos = rigid_body_.GetPosition();
		glm::vec3 vel = rigid_body_.GetLinearVelocity();

		// Simple flocking behaviors
		glm::vec3 separation(0.0f);
		glm::vec3 alignment(0.0f);
		glm::vec3 cohesion(0.0f);
		glm::vec3 avoidance(0.0f);
		int       count = 0;

		auto entities = handler.GetAllEntities();
		for (auto const& [id, other_base] : entities) {
			if (id == GetId())
				continue;
			auto other = std::dynamic_pointer_cast<SdfBoid>(other_base);
			if (!other)
				continue;

			glm::vec3 other_pos = other->GetPosition().Toglm();
			float     dist = glm::distance(pos, other_pos);

			if (other->IsPredator() && !is_predator_) {
				if (dist < 50.0f) {
					avoidance += glm::normalize(pos - other_pos) * (50.0f / (dist + 0.1f));
				}
				continue;
			}

			if (dist < 30.0f) {
				if (dist < 10.0f) {
					separation += glm::normalize(pos - other_pos) * (10.0f / (dist + 0.1f));
				}
				alignment += other->GetVelocity().Toglm();
				cohesion += other_pos;
				count++;
			}
		}

		glm::vec3 accel(0.0f);
		if (count > 0) {
			alignment /= (float)count;
			cohesion = (cohesion / (float)count) - pos;
			accel += separation * 5.0f;
			accel += alignment * 0.5f;
			accel += cohesion * 0.1f;
		}
		accel += avoidance * 10.0f;

		// Keep them within a volume
		if (glm::length(pos) > 100.0f) {
			accel += -glm::normalize(pos) * (glm::length(pos) - 100.0f) * 0.1f;
		}

		vel += accel * delta_time * 10.0f;

		float max_vel = is_predator_ ? 90.0f : 30.0f;
		if (glm::length(vel) > max_vel)
			vel = glm::normalize(vel) * max_vel;
		if (glm::length(vel) < 5.0f)
			vel = glm::normalize(vel) * 5.0f;

		rigid_body_.SetLinearVelocity(vel);
		rigid_body_.SetPosition(pos + vel * delta_time);

		// Orient to velocity
		if (glm::length(vel) > 0.001f) {
			glm::quat target_rot = glm::quatLookAt(glm::normalize(vel), glm::vec3(0, 1, 0));
			rigid_body_.SetOrientation(glm::slerp(rigid_body_.GetOrientation(), target_rot, delta_time * 5.0f));
		}
	}

	SdfBoidHandler::SdfBoidHandler(
		task_thread_pool::task_thread_pool& thread_pool,
		std::shared_ptr<Visualizer>&        visualizer
	):
		EntityHandler(thread_pool, visualizer) {
		std::random_device               rd;
		std::mt19937                     gen(rd());
		std::uniform_real_distribution<> dis(-80.0, 80.0);
		std::uniform_real_distribution<> dis_vel(-10.0, 10.0);

		// Add boids
		for (int i = 0; i < 50; ++i) {
			bool is_predator = (i % 2); // One predator
			auto boid = std::make_shared<SdfBoid>(is_predator);
			boid->SetPosition(dis(gen), dis(gen), dis(gen));
			boid->SetVelocity(dis_vel(gen), dis_vel(gen), dis_vel(gen));

			// Create SDF source
			SdfSource source;
			source.position = boid->GetPosition().Toglm();
			source.radius = is_predator ? 25.0f : 10.0f;
			source.color = is_predator ? glm::vec3(1.0f, 0.2f, 0.2f) : glm::vec3(0.2f, 0.6f, 1.0f);
			source.charge = is_predator ? -5.0f : 1.0f; // Predator cancels, others merge
			source.smoothness = 4.0f;
			source.type = 0; // Sphere

			int sid = visualizer->AddSdfSource(source);
			boid->SetSdfSourceId(sid);

			AddEntity(std::dynamic_pointer_cast<EntityBase>(boid));
		}
	}

	SdfBoidHandler::~SdfBoidHandler() {}

	void SdfBoidHandler::PostTimestep(float time, float delta_time) {
		(void)time;
		(void)delta_time;
	}

	void SdfBoidHandler::OnEntityUpdated(std::shared_ptr<EntityBase> entity) {
		auto boid = std::dynamic_pointer_cast<SdfBoid>(entity);
		if (boid && vis) {
			SdfSource source;
			source.position = boid->GetPosition().Toglm();
			source.radius = boid->IsPredator() ? 15.0f : 10.0f;
			source.color = boid->IsPredator() ? glm::vec3(1.0f, 0.2f, 0.2f) : glm::vec3(0.2f, 0.6f, 1.0f);
			source.charge = boid->IsPredator() ? -1.0f : 1.0f;
			source.smoothness = 4.0f;
			source.type = 0;

			vis->UpdateSdfSource(boid->GetSdfSourceId(), source);
		}
	}

} // namespace Boidsish
