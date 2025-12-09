#include "shape.h"

#include <memory>

#include "shader.h"

namespace Boidsish {
	std::shared_ptr<Shader> Shape::shader = nullptr;
}
