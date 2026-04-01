#include "scene_compositor.h"

#include <iostream>

#include "post_processing/PostProcessingManager.h"
#include "shader.h"
#include "shockwave_effect.h"

namespace Boidsish {

	SceneCompositor::SceneCompositor(int render_width, int render_height, bool enable_hdr, GLuint blit_vao):
		blit_vao_(blit_vao),
		render_width_(render_width),
		render_height_(render_height),
		hdr_(enable_hdr) {

		blit_shader_ = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");

		glGenFramebuffers(1, &main_fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, main_fbo_);

		CreateTextures();

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "ERROR::FRAMEBUFFER:: Main framebuffer is not complete!" << std::endl;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	SceneCompositor::~SceneCompositor() {
		if (main_fbo_) {
			glDeleteFramebuffers(1, &main_fbo_);
			glDeleteTextures(1, &color_tex_);
			glDeleteTextures(1, &velocity_tex_);
			glDeleteTextures(1, &depth_tex_);
			if (refraction_tex_) glDeleteTextures(1, &refraction_tex_);
		}
	}

	void SceneCompositor::CreateTextures() {
		// Color attachment
		glGenTextures(1, &color_tex_);
		glBindTexture(GL_TEXTURE_2D, color_tex_);
		if (hdr_) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, render_width_, render_height_, 0, GL_RGB, GL_FLOAT, NULL);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_width_, render_height_, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex_, 0);

		// Refraction texture (not attached to FBO, copied into via glCopyTexSubImage2D)
		glGenTextures(1, &refraction_tex_);
		glBindTexture(GL_TEXTURE_2D, refraction_tex_);
		if (hdr_) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, render_width_, render_height_, 0, GL_RGB, GL_FLOAT, NULL);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_width_, render_height_, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Velocity attachment
		glGenTextures(1, &velocity_tex_);
		glBindTexture(GL_TEXTURE_2D, velocity_tex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, render_width_, render_height_, 0, GL_RG, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, velocity_tex_, 0);

		// Depth-stencil texture
		glGenTextures(1, &depth_tex_);
		glBindTexture(GL_TEXTURE_2D, depth_tex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, render_width_, render_height_, 0,
		             GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth_tex_, 0);
	}

	void SceneCompositor::BeginScene(const FrameData& frame, float render_scale) {
		bool effects_enabled = frame.config.effects_enabled;
		bool has_shockwaves = frame.has_shockwaves;
		bool skip_intermediate = (render_scale == 1.0f && !effects_enabled && !has_shockwaves);

		if (skip_intermediate) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, frame.window_width, frame.window_height);
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, main_fbo_);
			glViewport(0, 0, render_width_, render_height_);
			GLuint attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
			glDrawBuffers(2, attachments);
		}

		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void SceneCompositor::CaptureRefraction(const FrameData& frame, bool effects_enabled, GLuint post_fx_fbo) {
		if (effects_enabled && post_fx_fbo != 0) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, post_fx_fbo);
		} else {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, main_fbo_);
		}
		glBindTexture(GL_TEXTURE_2D, refraction_tex_);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, render_width_, render_height_);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	void SceneCompositor::ResolveToScreen(
		const FrameData& frame,
		float render_scale,
		PostProcessing::PostProcessingManager* post_fx,
		ShockwaveManager* shockwave
	) {
		bool effects_enabled = frame.config.effects_enabled;
		bool has_shockwaves = frame.has_shockwaves;

		if (effects_enabled && post_fx) {
			post_fx->ApplyLateEffects(frame.view, frame.projection, frame.camera_pos, frame.simulation_time);
			GLuint final_texture = post_fx->GetFinalTexture();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, frame.window_width, frame.window_height);
			glDisable(GL_DEPTH_TEST);
			glClear(GL_COLOR_BUFFER_BIT);

			if (has_shockwaves && shockwave) {
				shockwave->ApplyScreenSpaceEffect(
					final_texture, depth_tex_, frame.view, frame.projection,
					frame.camera_pos, blit_vao_, frame.window_width, frame.window_height
				);
			} else {
				blit_shader_->use();
				blit_shader_->setInt("screenTexture", 0);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, final_texture);
				glBindVertexArray(blit_vao_);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		} else {
			bool skip_intermediate = (render_scale == 1.0f && !has_shockwaves);

			if (!skip_intermediate) {
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glViewport(0, 0, frame.window_width, frame.window_height);
				glDisable(GL_DEPTH_TEST);
				glClear(GL_COLOR_BUFFER_BIT);

				if (has_shockwaves && shockwave) {
					shockwave->ApplyScreenSpaceEffect(
						color_tex_, depth_tex_, frame.view, frame.projection,
						frame.camera_pos, blit_vao_, frame.window_width, frame.window_height
					);
				} else {
					glBindFramebuffer(GL_READ_FRAMEBUFFER, main_fbo_);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
					glBlitFramebuffer(
						0, 0, render_width_, render_height_,
						0, 0, frame.window_width, frame.window_height,
						GL_COLOR_BUFFER_BIT, GL_LINEAR
					);
				}
			}
		}
	}

	void SceneCompositor::Resize(int render_width, int render_height, bool enable_hdr) {
		render_width_ = render_width;
		render_height_ = render_height;
		hdr_ = enable_hdr;

		// Resize color
		glBindTexture(GL_TEXTURE_2D, color_tex_);
		if (hdr_) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, render_width_, render_height_, 0, GL_RGB, GL_FLOAT, NULL);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_width_, render_height_, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		}

		// Resize velocity
		glBindTexture(GL_TEXTURE_2D, velocity_tex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, render_width_, render_height_, 0, GL_RG, GL_FLOAT, NULL);

		// Resize refraction
		glBindTexture(GL_TEXTURE_2D, refraction_tex_);
		if (hdr_) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, render_width_, render_height_, 0, GL_RGB, GL_FLOAT, NULL);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_width_, render_height_, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		}

		// Resize depth-stencil
		glBindTexture(GL_TEXTURE_2D, depth_tex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, render_width_, render_height_, 0,
		             GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	}

} // namespace Boidsish
