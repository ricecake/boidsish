#include "post_processing/effects/OpticalFlowEffect.h"

#include <iostream>

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		OpticalFlowEffect::OpticalFlowEffect() {
			name_ = "OpticalFlow";
		}

		OpticalFlowEffect::~OpticalFlowEffect() {
			CleanupFBO();
		}

		void OpticalFlowEffect::Initialize(int width, int height) {
			_shader = std::make_unique<Shader>(
				"shaders/post_processing/optical_flow.vert",
				"shaders/post_processing/optical_flow.frag"
			);
			_passthroughShader = std::make_unique<Shader>(
				"shaders/post_processing/optical_flow.vert",
				"shaders/post_processing/passthrough.frag"
			);
			Resize(width, height);
		}

		void OpticalFlowEffect::CleanupFBO() {
			glDeleteFramebuffers(1, &_previousFrameFBO);
			glDeleteTextures(1, &_previousFrameTexture);
			glDeleteFramebuffers(2, _flowFBOs);
			glDeleteTextures(2, _flowTextures);
			glDeleteFramebuffers(1, &_outputFBO);
			glDeleteTextures(1, &_outputTexture);
		}

		void OpticalFlowEffect::InitializeFBO(int width, int height) {
			_width = width;
			_height = height;

			CleanupFBO();

			// Previous Frame FBO
			glGenFramebuffers(1, &_previousFrameFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, _previousFrameFBO);
			glGenTextures(1, &_previousFrameTexture);
			glBindTexture(GL_TEXTURE_2D, _previousFrameTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _previousFrameTexture, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cerr << "ERROR::FRAMEBUFFER:: Previous Frame FBO is not complete!" << std::endl;

			// Optical Flow FBOs (Ping-Pong)
			for (int i = 0; i < 2; ++i) {
				glGenFramebuffers(1, &_flowFBOs[i]);
				glBindFramebuffer(GL_FRAMEBUFFER, _flowFBOs[i]);
				glGenTextures(1, &_flowTextures[i]);
				glBindTexture(GL_TEXTURE_2D, _flowTextures[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _flowTextures[i], 0);
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
					std::cerr << "ERROR::FRAMEBUFFER:: Flow FBO " << i << " is not complete!" << std::endl;
			}

			// Output FBO
			glGenFramebuffers(1, &_outputFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, _outputFBO);
			glGenTextures(1, &_outputTexture);
			glBindTexture(GL_TEXTURE_2D, _outputTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _outputTexture, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cerr << "ERROR::FRAMEBUFFER:: Output FBO is not complete!" << std::endl;

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void OpticalFlowEffect::Resize(int width, int height) {
			InitializeFBO(width, height);
		}

		void OpticalFlowEffect::Apply(GLuint sourceTexture) {
			// 1. Get the currently bound FBO to restore it later
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);

			// 2. Calculate optical flow and acceleration into our own FBO.
			glBindFramebuffer(GL_FRAMEBUFFER, _outputFBO);
			GLuint attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
			glDrawBuffers(2, attachments);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _outputTexture, 0);
			glFramebufferTexture2D(
				GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT1,
				GL_TEXTURE_2D,
				_flowTextures[1 - _currentFlowIndex],
				0
			);

			glClear(GL_COLOR_BUFFER_BIT);

			_shader->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _previousFrameTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, _flowTextures[_currentFlowIndex]);

			_shader->setInt("u_current_texture", 0);
			_shader->setInt("u_previous_texture", 1);
			_shader->setInt("u_previous_flow_texture", 2);
			_shader->setVec2("u_texel_size", 1.0f / _width, 1.0f / _height);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			_currentFlowIndex = 1 - _currentFlowIndex;

			// 3. Save current frame for the next iteration into our history FBO.
			glBindFramebuffer(GL_FRAMEBUFFER, _previousFrameFBO);
			glClear(GL_COLOR_BUFFER_BIT);
			_passthroughShader->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			_passthroughShader->setInt("u_texture", 0);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 4. Restore the original FBO and render the result to it.
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			_passthroughShader->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, _outputTexture);
			_passthroughShader->setInt("u_texture", 0);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

	} // namespace PostProcessing
} // namespace Boidsish
