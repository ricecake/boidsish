#include "missile_launcher.h"
#include "spatial_entity_handler.h"

namespace Boidsish {

MissileLauncher::MissileLauncher(int id, Vector3 pos)
    : Entity<Model>(id, "assets/utah_teapot.obj", false),
      eng_(rd_()) {
    SetPosition(pos.x, pos.y, pos.z);
    SetColor(0.8f, 0.1f, 0.1f);
    shape_->SetScale(glm::vec3(2.0f));
    time_since_last_fire_ = 0.0f; // Start with a full cooldown

    // Randomize the firing interval
    std::uniform_real_distribution<float> dist(4.0f, 8.0f);
    fire_interval_ = dist(eng_);
}

void MissileLauncher::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    time_since_last_fire_ += delta_time;
    if (time_since_last_fire_ < fire_interval_) {
        return;
    }

    auto planes = handler.GetEntitiesByType<PaperPlane>();
    if (planes.empty()) return;
    auto plane = planes[0];

    // Check distance to plane
    float distance_to_plane = (plane->GetPosition() - GetPosition()).Magnitude();
    if (distance_to_plane > 500.0f) {
        return;
    }

    // Calculate firing probability based on altitude
    auto  ppos = plane->GetPosition();
    float max_h = handler.GetTerrainMaxHeight();
    if (max_h <= 0.0f) max_h = 200.0f; // Fallback

    float start_h = 60.0f;
    float extreme_h = 3.0f * max_h;

    if (ppos.y < start_h) return;

    const float p_min = 0.5f;
    const float p_max = 10.0f;

    float norm_alt = (ppos.y - start_h) / (extreme_h - start_h);
    norm_alt = std::min(std::max(norm_alt, 0.0f), 1.0f);

    float missiles_per_second = p_min + (p_max - p_min) * norm_alt;
    float fire_probability_this_frame = missiles_per_second * delta_time;

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    if (dist(eng_) < fire_probability_this_frame) {
        handler.QueueAddEntity<GuidedMissile>(GetPosition());
        time_since_last_fire_ = 0.0f;
        // Set a new random interval for the next shot
        std::uniform_real_distribution<float> new_dist(4.0f, 8.0f);
        fire_interval_ = new_dist(eng_);
    }
}

}
