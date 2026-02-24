#include "GuidedMissileLauncher.h"

#include <algorithm>

#include "GuidedMissile.h"
#include "KittywumpusPlane.h"
#include "KittywumpusHandler.h"
#include "arcade_text.h"
#include "graphics.h"
#include "terrain_generator_interface.h"
#include <glm/gtc/constants.hpp>

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

		UpdateShape();
	}

	void GuidedMissileLauncher::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (!approach_point_set_) {
			auto  pos = GetPosition().Toglm();
			float max_neighbor_h = pos.y;
			float sample_dist = 50.0f;

			for (int i = 0; i < 8; ++i) {
				float     angle = i * (glm::pi<float>() / 4.0f);
				glm::vec3 dir(sin(angle), 0, cos(angle));
				auto [h, norm] = handler.GetTerrainPropertiesAtPoint(
					pos.x + dir.x * sample_dist,
					pos.z + dir.z * sample_dist
				);
				max_neighbor_h = std::max(max_neighbor_h, h);
			}

			approach_point_ = pos + glm::vec3(0, std::max(30.0f, (max_neighbor_h - pos.y) + 20.0f), 0);
			approach_point_set_ = true;
		}

		// if (!text_) {
		// 	handler.EnqueueVisualizerAction([&handler, this]() {
		// 		auto camPos = handler.vis->GetCamera().pos();
		// 		auto pos = this->GetPosition().Toglm();
		// 		auto vec = pos - camPos;

		// 		text_ = handler.vis->AddArcadeTextEffect(
		// 			"SUP",
		// 			pos,
		// 			20.0f,
		// 			60.0f,
		// 			glm::vec3(0, 1, 0),
		// 			-1 * vec,
		// 			100.0f,
		// 			"assets/Roboto-Medium.ttf",
		// 			12.0f,
		// 			2.0f

		// 		);
		// 		text_->SetPulseSpeed(3.0f);
		// 		text_->SetPulseAmplitude(0.3f);
		// 		text_->SetRainbowEnabled(true);
		// 		text_->SetRainbowSpeed(5.0f);
		// 	});
		// }

		time_since_last_fire_ += delta_time;
		if (time_since_last_fire_ < fire_interval_) {
			return;
		}

		auto planes = handler.GetEntitiesByType<KittywumpusPlane>();
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
			max_h = 300.0f;

		float start_h = 70.0f;
		float extreme_h = 3.0f * max_h;

		if (ppos.y < start_h)
			return;

		const float p_min = 0.4f;
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
			if (handler.GetEntitiesByType<GuidedMissile>().size() < kMaxInFlightMissiles) {
				handler.QueueAddEntity<GuidedMissile>(GetPosition());
				time_since_last_fire_ = 0.0f;
				std::uniform_real_distribution<float> new_dist(4.0f, 8.0f);
				fire_interval_ = new_dist(eng_);
			}
		}
	}

	void GuidedMissileLauncher::OnHit(const EntityHandler& handler, float damage) {
		(void)damage;
		Destroy(handler);
	}

	void GuidedMissileLauncher::Destroy(const EntityHandler& handler) {
		// Award points for destroying the launcher
		if (auto* pp_handler = dynamic_cast<const KittywumpusHandler*>(&handler)) {
			pp_handler->AddScore(500, "Launcher Destroyed");
		}

		auto pos = GetPosition().Toglm();
		auto [h_val, n_val] = handler.GetTerrainPropertiesAtPoint(pos.x, pos.z);
		float     height = h_val;
		glm::vec3 normal = n_val;

		// if (!text_) {
		handler.EnqueueVisualizerAction([&handler, this, normal, pos, height]() {
			handler.vis->TriggerComplexExplosion(this->shape_, normal, 2.0f, FireEffectStyle::Explosion);
			handler.vis->GetTerrain()->AddCrater({pos.x, height, pos.z}, 15.0f, 8.0f, 0.2f, 2.0f);

			auto camPos = handler.vis->GetCamera().pos();
			auto pos = this->GetPosition().Toglm();
			auto vec = pos - camPos;

			text_ = handler.vis->AddArcadeTextEffect(
				"GOT EM'",
				pos,
				20.0f,
				60.0f,
				glm::vec3(0, 1, 0),
				-1 * vec,
				3.0f,
				"assets/Roboto-Medium.ttf",
				12.0f,
				2.0f

			);
			text_->SetPulseSpeed(3.0f);
			text_->SetPulseAmplitude(0.3f);
			text_->SetRainbowEnabled(true);
			text_->SetRainbowSpeed(5.0f);

			handler.QueueRemoveEntity(GetId());
		});
		// }
	}

} // namespace Boidsish
