#pragma once

#include "PaperPlane.h"
#include "ShieldBoid.h"
#include "entity.h"
#include "steering_probe.h"

namespace Boidsish {

	class SteeringProbeEntity: public Entity<Dot> {
	public:
		SteeringProbeEntity(int id, std::shared_ptr<ITerrainGenerator> terrain, std::shared_ptr<PaperPlane> player):
			Entity<Dot>(id), player_(player) {
			probe_ = std::make_shared<SteeringProbe>(terrain);

			// Visuals: Shiny silver orb
			SetColor(0.75f, 0.75f, 0.75f);
			SetUsePBR(true);
			SetMetallic(1.0f);
			SetRoughness(0.1f);
			SetSize(940.0f);

			// Initial state
			if (player) {
				probe_->SetPosition(player->GetPosition().Toglm() + player->GetVelocity().Toglm());
				probe_->SetVelocity(player->GetVelocity().Toglm());
			}
		}

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
			(void)time;
			if (!player_)
				return;

			probe_->Update(delta_time, player_->GetPosition().Toglm(), player_->GetVelocity().Toglm());
			auto p = probe_->GetPosition();
			SetPosition(p.x, p.y, p.z);

			int checkpoint_id = probe_->HandleCheckpoints(delta_time, handler, player_);
			if (checkpoint_id != -1) {
				auto cp = handler.GetEntity(checkpoint_id);
				if (cp) {
					glm::vec3 cp_pos = cp->GetPosition().Toglm();
					for (int i = 0; i < 3; ++i) {
						handler.QueueAddEntity<ShieldBoid>(cp_pos);
					}
				}
			}

			UpdateShape();
		}

	private:
		std::shared_ptr<SteeringProbe> probe_;
		std::shared_ptr<PaperPlane>    player_;
	};

} // namespace Boidsish
