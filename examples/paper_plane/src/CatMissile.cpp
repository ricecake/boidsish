#include "CatMissile.h"

namespace Boidsish {

CatMissile::CatMissile(
    int id,
    Vector3 pos,
    glm::quat orientation,
    glm::vec3 dir,
    Vector3 vel
) : SeekingMissile<PaperPlane, CatMissileFlightParams>(id, pos) {
    // Set initial state specific to CatMissile
    this->orientation_ = orientation;
    auto netVelocity = glm::vec3(vel.x, vel.y, vel.z) + 5.0f * glm::normalize(glm::vec3(dir.x, dir.y, dir.z));
    SetVelocity(netVelocity.x, netVelocity.y, netVelocity.z);

    // Override base settings for the pre-launch phase
    SetTrailLength(0);
    SetTrailRocket(false);
    shape_->SetScale(glm::vec3(0.05f));
    std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
        glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
    );
    UpdateShape();
}

void CatMissile::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    // The base class UpdateEntity handles the lifetime/explosion checks,
    // but CatMissile has custom logic for its launch phase.

    // Custom pre-launch behavior for CatMissile (gravity drop)
    if (lived_ < CatMissileFlightParams::kLaunchTime) {
        auto velo = GetVelocity();
        velo += Vector3(0, -0.07f, 0);
        SetVelocity(velo);
        // We do *not* call the base UpdateEntity here.
    } else {
        // Post-launch: activate trail and then let the base class handle seeking.
        if (!fired_) {
            SetTrailLength(500);
            SetTrailRocket(true);
            fired_ = true;
        }

        // The base class implementation will re-check the lifetime and launch time,
        // then execute the seeking logic, which is what we want.
        // It will also handle applying the final velocity and orientation.
        SeekingMissile<PaperPlane, CatMissileFlightParams>::UpdateEntity(handler, time, delta_time);
    }
}

} // namespace Boidsish
