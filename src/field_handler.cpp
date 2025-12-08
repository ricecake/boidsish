#include "field_handler.h"

namespace Boidsish {

void VectorFieldHandler::PreTimestep(float time, float delta_time) {
    (void)time;
    (void)delta_time;
    for (auto const& [name, val] : fields_) {
        (void)name;
        val[1 - current_field_]->Clear();
    }
}

void VectorFieldHandler::PostTimestep(float time, float delta_time) {
    (void)time;
    (void)delta_time;
    SwapFields();
}

}
