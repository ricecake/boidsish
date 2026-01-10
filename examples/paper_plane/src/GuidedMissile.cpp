#include "GuidedMissile.h"

namespace Boidsish {

GuidedMissile::GuidedMissile(int id, Vector3 pos)
    : SeekingMissile<PaperPlane, GuidedMissileFlightParams>(id, pos) {
    // Specific adjustments for GuidedMissile can be made here if necessary.
    // For example, if it used a different model or scale.
    // In this case, the base class constructor handles everything.
}

} // namespace Boidsish
