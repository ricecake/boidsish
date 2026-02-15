#pragma once

#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		struct GBuffer {
			GLuint depth;
			GLuint normal;
			GLuint material;
			GLuint velocity;
			GLuint hiz;
			int    hiz_levels;
		};

	} // namespace PostProcessing
} // namespace Boidsish
