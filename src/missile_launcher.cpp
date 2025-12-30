#include "missile_launcher.h"
#include "guided_missile.h"
#include "paper_plane.h"
#include "graphics.h"

#include <glm/gtc/quaternion.hpp>
#include <random>

namespace Boidsish {

MissileLauncher::MissileLauncher(int id, const glm::vec3& position)
    : Entity<Model>(id, "assets/utah_teapot.obj", false),
      cooldown_(0.0f),
      eng_(rd_()),
      dist_(0.0f, 1.0f) {
    SetPosition(position.x, position.y, position.z);
    shape_->SetScale(glm::vec3(2.0f)); // Make the teapot a reasonable size
    UpdateShape();
}

void MissileLauncher::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    if (cooldown_ > 0.0f) {
        cooldown_ -= delta_time;
        return;
    }

    auto targets = handler.GetEntitiesByType<PaperPlane>();
    if (targets.empty()) {
        return;
    }

    auto plane = targets[0];
    auto ppos = plane->GetPosition();

    // Firing probability logic
    float start_h = 60.0f;
    float extreme_h = 300.0f;

    if (ppos.y < start_h) {
        return;
    }

    const float p_min = 0.5f;
    const float p_max = 5.0f;

    float norm_alt = (ppos.y - start_h) / (extreme_h - start_h);
    norm_alt = std::min(std::max(norm_alt, 0.0f), 1.0f); // clamp

    float missiles_per_second = p_min * pow((p_max / p_min), norm_alt);
    float fire_probability_this_frame = missiles_per_second * delta_time;

    if (dist_(eng_) < fire_probability_this_frame) {
        auto missile_pos = GetPosition();
        handler.QueueAddEntity<GuidedMissile>(missile_pos);
        cooldown_ = kFiringCooldown_;
    }
}

} // namespace Boidsish
