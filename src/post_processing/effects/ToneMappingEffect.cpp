#include "post_processing/effects/ToneMappingEffect.h"

#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		ToneMappingEffect::ToneMappingEffect() {
			name_ = "ToneMapping";
		}

		ToneMappingEffect::~ToneMappingEffect() {
			glDeleteFramebuffers(_mipChainFBO.size(), _mipChainFBO.data());
			glDeleteTextures(_mipChainTexture.size(), _mipChainTexture.data());
			glDeleteFramebuffers(2, _lumPingPongFBO);
			glDeleteTextures(2, _lumPingPongTexture);
		}

		void ToneMappingEffect::Initialize(int width, int height) {
			_shader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/tonemapping.frag");
			_downsampleShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/downsample.frag");
			_adaptationShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/adaptation.frag");
			width_ = width;
			height_ = height;

			// Mipmap chain for luminance calculation
			int mip_width = width;
			int mip_height = height;
			while (mip_width > 1 || mip_height > 1) {
				mip_width /= 2;
				mip_height /= 2;
				if (mip_width < 1) mip_width = 1;
				if (mip_height < 1) mip_height = 1;

				GLuint fbo, texture;
				glGenFramebuffers(1, &fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, fbo);
				glGenTextures(1, &texture);
				glBindTexture(GL_TEXTURE_2D, texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, mip_width, mip_height, 0, GL_RED, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
				_mipChainFBO.push_back(fbo);
				_mipChainTexture.push_back(texture);
			}

			// Ping-pong buffers for luminance
			for (int i = 0; i < 2; i++) {
				glGenFramebuffers(1, &_lumPingPongFBO[i]);
				glBindFramebuffer(GL_FRAMEBUFFER, _lumPingPongFBO[i]);
				glGenTextures(1, &_lumPingPongTexture[i]);
				glBindTexture(GL_TEXTURE_2D, _lumPingPongTexture[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 1, 1, 0, GL_RG, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _lumPingPongTexture[i], 0);

				// Initialize with default exposure of 1.0
				glClearColor(0.5f, 1.0f, 0.0f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
			}
		}

		void ToneMappingEffect::Apply(GLuint sourceTexture, float delta_time) {
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);
			GLint originalViewport[4];
			glGetIntegerv(GL_VIEWPORT, originalViewport);

			// --- Luminance calculation ---
			_downsampleShader->use();
			_downsampleShader->setInt("sourceTexture", 0);
			glActiveTexture(GL_TEXTURE0);

			GLuint current_texture = sourceTexture;
			int mip_width = width_;
			int mip_height = height_;
			for (size_t i = 0; i < _mipChainFBO.size(); i++) {
				mip_width /= 2;
				mip_height /= 2;
				if (mip_width < 1) mip_width = 1;
				if (mip_height < 1) mip_height = 1;

				glBindFramebuffer(GL_FRAMEBUFFER, _mipChainFBO[i]);
				glViewport(0, 0, mip_width, mip_height);
				glBindTexture(GL_TEXTURE_2D, current_texture);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				current_texture = _mipChainTexture[i];
			}

			// --- Adaptation ---
			int read_index = _lumPingPongIndex;
			int write_index = (_lumPingPongIndex + 1) % 2;
			glBindFramebuffer(GL_FRAMEBUFFER, _lumPingPongFBO[write_index]);
			glViewport(0, 0, 1, 1);

			_adaptationShader->use();
			_adaptationShader->setInt("lumTexture", 0);
			_adaptationShader->setInt("lastFrameLumTexture", 1);
			_adaptationShader->setFloat("deltaTime", delta_time);
			_adaptationShader->setFloat("adaptationSpeedUp", _adaptationSpeedUp);
			_adaptationShader->setFloat("adaptationSpeedDown", _adaptationSpeedDown);
			_adaptationShader->setFloat("targetLuminance", _targetLuminance);
			_adaptationShader->setVec2("exposureClamp", _exposureClamp);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, current_texture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _lumPingPongTexture[read_index]);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			_lumPingPongIndex = write_index;

			// --- Tone mapping ---
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			glViewport(0, 0, width_, height_);

			_shader->use();
			_shader->setInt("sceneTexture", 0);
			_shader->setInt("lumTexture", 1);
			_shader->setInt("toneMapMode", toneMode_);
			_shader->setVec2("resolution", (float)width_, (float)height_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _lumPingPongTexture[write_index]);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			glViewport(originalViewport[0], originalViewport[1], originalViewport[2], originalViewport[3]);
		}

		void ToneMappingEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			glDeleteFramebuffers(_mipChainFBO.size(), _mipChainFBO.data());
			glDeleteTextures(_mipChainTexture.size(), _mipChainTexture.data());
			glDeleteFramebuffers(2, _lumPingPongFBO);
			glDeleteTextures(2, _lumPingPongTexture);
			_mipChainFBO.clear();
			_mipChainTexture.clear();
			Initialize(width, height);
		}

	} // namespace PostProcessing
} // namespace Boidsish
