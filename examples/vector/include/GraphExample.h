#pragma once

#include <memory>
#include <vector>

namespace Boidsish {

	class Shape; // Forward declaration

	std::vector<std::shared_ptr<Shape>> GraphExample(float time);

} // namespace Boidsish
