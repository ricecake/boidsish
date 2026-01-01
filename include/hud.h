#pragma once

#include <string>

#include <glm/glm.hpp>

namespace Boidsish {

	enum class HudAlignment {
		TOP_LEFT,
		TOP_CENTER,
		TOP_RIGHT,
		MIDDLE_LEFT,
		MIDDLE_CENTER,
		MIDDLE_RIGHT,
		BOTTOM_LEFT,
		BOTTOM_CENTER,
		BOTTOM_RIGHT
	};

	struct HudIcon {
		int          id;
		std::string  texture_path;
		HudAlignment alignment;
		glm::vec2    position; // Offset from alignment point
		glm::vec2    size;
		bool         highlighted = false;
	};

	struct HudNumber {
		int          id;
		float        value;
		std::string  label;
		HudAlignment alignment;
		glm::vec2    position;
		int          precision = 0;
	};

	struct HudGauge {
		int          id;
		float        value; // Should be in the range [0.0, 1.0]
		std::string  label;
		HudAlignment alignment;
		glm::vec2    position;
		glm::vec2    size;
	};

} // namespace Boidsish
