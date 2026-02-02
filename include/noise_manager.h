#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include "shader.h"

namespace Boidsish {

	class NoiseManager {
	public:
		NoiseManager();
		~NoiseManager();

		void Initialize();
		void Update(float time);

		void BindTexture(GLuint unit = 5);
		GLuint GetTextureID() const { return m_noiseTexture; }

	private:
		GLuint m_noiseTexture = 0;
		std::unique_ptr<ComputeShader> m_computeShader;
		bool m_initialized = false;

		static constexpr int TEXTURE_SIZE = 128;
	};

} // namespace Boidsish
