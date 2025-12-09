#include "shape.h"

#include "shader.h"

#include <memory>

namespace Boidsish {
    std::shared_ptr<Shader> Shape::shader = nullptr;
}
