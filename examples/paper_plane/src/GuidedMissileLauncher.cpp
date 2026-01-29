#include "GuidedMissileLauncher.h"

#include <algorithm>

#include "GuidedMissile.h"
#include "PaperPlane.h"
#include "graphics.h"

namespace Boidsish {

	GuidedMissileLauncher::GuidedMissileLauncher(int id, Vector3 pos, glm::quat orientation):
		// Entity<Model>(id, "assets/utah_teapot.obj", false), eng_(rd_()) {
		Entity<Model>(id, "assets/quickMissileLauncher.obj", false), eng_(rd_()) {
		SetPosition(pos.x, pos.y, pos.z);
		shape_->SetScale(glm::vec3(0.50f));
		// shape_->SetRotation(orientation);
		SetOrientation(orientation);

		std::uniform_real_distribution<float> dist(4.0f, 8.0f);
		fire_interval_ = dist(eng_);

		shape_->SetInstanced(true);

		UpdateShape();
	}

	void GuidedMissileLauncher::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		time_since_last_fire_ += delta_time;
		if (time_since_last_fire_ < fire_interval_) {
			return;
		}

		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (planes.empty())
			return;
		auto plane = planes[0];

		float distance_to_plane = (plane->GetPosition() - GetPosition()).Magnitude();
		if (distance_to_plane > 500.0f) {
			return;
		}

		auto  pos = GetPosition();
		auto  ppos = plane->GetPosition();
		auto  pvel = plane->GetVelocity();
		float max_h = handler.vis->GetTerrainMaxHeight();

		if (max_h <= 0.0f)
			max_h = 200.0f;

		float start_h = 60.0f;
		float extreme_h = 3.0f * max_h;

		if (ppos.y < start_h)
			return;

		const float p_min = 0.5f;
		const float p_max = 10.0f;

		glm::vec3 pvel_n = (glm::length(pvel.Toglm()) > 0.001f) ? glm::normalize(pvel) : glm::vec3(0, 0, 1);
		glm::vec3 to_launcher_n = (glm::length(pos.Toglm() - ppos) > 0.001f) ? glm::normalize(pos.Toglm() - ppos)
																			 : glm::vec3(0, 1, 0);
		auto      directionWeight = glm::dot(pvel_n, to_launcher_n);

		float norm_alt = (ppos.y - start_h) / (extreme_h - start_h);
		norm_alt = std::min(std::max(norm_alt, 0.0f), 1.0f);

		float missiles_per_second = p_min + (p_max - p_min) * norm_alt;

		// Ensure we have a minimum firing probability even if the plane is flying away,
		// and handle potential zero velocity/direction vectors
		float effectiveWeight = std::max(0.1f, directionWeight);
		if (std::isnan(effectiveWeight))
			effectiveWeight = 0.1f;

		float fire_probability_this_frame = missiles_per_second * effectiveWeight * delta_time;

		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		if (dist(eng_) < fire_probability_this_frame) {
			handler.QueueAddEntity<GuidedMissile>(GetPosition());
			time_since_last_fire_ = 0.0f;
			std::uniform_real_distribution<float> new_dist(4.0f, 8.0f);
			fire_interval_ = new_dist(eng_);
		}
	}

} // namespace Boidsish
