#include "noise_manager.h"
#include "logger.h"
#include <vector>

namespace Boidsish {

	NoiseManager::NoiseManager() {}

	NoiseManager::~NoiseManager() {
		if (m_noiseTexture != 0) {
			glDeleteTextures(1, &m_noiseTexture);
		}
	}

	void NoiseManager::Initialize() {
		if (m_initialized) return;

		// Check if compute shaders are supported by looking at OpenGL version
		GLint major, minor;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MINOR_VERSION, &minor);
		if (major < 4 || (major == 4 && minor < 3)) {
			logger::ERROR("Compute shaders not supported (need 4.3+, have " + std::to_string(major) + "." + std::to_string(minor) + ")");
			return;
		}

		// Create 3D texture
		glGenTextures(1, &m_noiseTexture);
		glBindTexture(GL_TEXTURE_3D, m_noiseTexture);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, TEXTURE_SIZE, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

		// Load compute shader
		m_computeShader = std::make_unique<ComputeShader>("shaders/noise_gen.comp");
		if (!m_computeShader->isValid()) {
			logger::ERROR("Failed to load noise compute shader");
			return;
		}

		m_initialized = true;
		logger::INFO("NoiseManager initialized");

		// Initial generation
		Update(0.0f);
	}

	void NoiseManager::Update(float /*time*/) {
		if (!m_initialized || !m_computeShader)
			return;

		m_computeShader->use();
		// m_computeShader->setFloat("u_time", time);

		glBindImageTexture(0, m_noiseTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

		m_computeShader->dispatch(TEXTURE_SIZE / 4, TEXTURE_SIZE / 4, TEXTURE_SIZE / 4);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	void NoiseManager::BindTexture(GLuint unit) {
		if (!m_initialized) return;
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_3D, m_noiseTexture);
	}

} // namespace Boidsish
