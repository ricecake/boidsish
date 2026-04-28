#include "scene_compositor.h"

#include <iostream>

#include "post_processing/PostProcessingManager.h"
#include "shader.h"
#include "shockwave_effect.h"

namespace Boidsish {

	SceneCompositor::SceneCompositor(int render_width, int render_height, bool enable_hdr, GLuint blit_vao):
		blit_vao_(blit_vao), render_width_(render_width), render_height_(render_height), hdr_(enable_hdr) {
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
		}
	}

	void SceneCompositor::CreateTextures() {
		// Color attachment
		GLint color_format = hdr_ ? GL_RGB16F : GL_RGB8;
		color_tex_ = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, color_format, render_width_, render_height_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex_->GetId(), 0);

		// Refraction texture (not attached to FBO, copied into via glCopyTexSubImage2D)
		refraction_tex_ = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, color_format, render_width_, render_height_);

		// Velocity attachment (RG = velocity, B = roughness, A = metallic)
		velocity_tex_ = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, GL_RGBA16F, render_width_, render_height_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, velocity_tex_->GetId(), 0);

		// Normal attachment
		normal_tex_ = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, GL_RGBA16F, render_width_, render_height_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, normal_tex_->GetId(), 0);

		// Albedo attachment
		albedo_tex_ = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, GL_RGBA16F, render_width_, render_height_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, albedo_tex_->GetId(), 0);

		// Depth-stencil texture
		depth_tex_ = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, GL_DEPTH24_STENCIL8, render_width_, render_height_);
		depth_tex_->SetParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		depth_tex_->SetParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth_tex_->GetId(), 0);
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
			GLuint attachments[4] = {
				GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1,
				GL_COLOR_ATTACHMENT2,
				GL_COLOR_ATTACHMENT3
			};
			glDrawBuffers(4, attachments);
		}

		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void SceneCompositor::CaptureRefraction(const FrameData& /*frame*/, bool effects_enabled, GLuint post_fx_fbo) {
		if (effects_enabled && post_fx_fbo != 0) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, post_fx_fbo);
		} else {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, main_fbo_);
		}
		glBindTexture(GL_TEXTURE_2D, refraction_tex_->GetId());
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, render_width_, render_height_);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	void SceneCompositor::ResolveToScreen(
		const FrameData&                       frame,
		float                                  render_scale,
		PostProcessing::PostProcessingManager* post_fx,
		ShockwaveManager*                      shockwave
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

			bool shockwave_applied = false;
			if (has_shockwaves && shockwave) {
				shockwave_applied = shockwave->ApplyScreenSpaceEffect(
					final_texture,
					depth_tex_->GetId(),
					frame.view,
					frame.projection,
					frame.camera_pos,
					blit_vao_,
					frame.window_width,
					frame.window_height
				);
			}

			if (!shockwave_applied) {
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

				bool shockwave_applied = false;
				if (has_shockwaves && shockwave) {
					shockwave_applied = shockwave->ApplyScreenSpaceEffect(
						color_tex_->GetId(),
						depth_tex_->GetId(),
						frame.view,
						frame.projection,
						frame.camera_pos,
						blit_vao_,
						frame.window_width,
						frame.window_height
					);
				}

				if (!shockwave_applied) {
					glBindFramebuffer(GL_READ_FRAMEBUFFER, main_fbo_);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
					glBlitFramebuffer(
						0,
						0,
						render_width_,
						render_height_,
						0,
						0,
						frame.window_width,
						frame.window_height,
						GL_COLOR_BUFFER_BIT,
						GL_LINEAR
					);
				}
			}
		}
	}

	void SceneCompositor::Resize(int render_width, int render_height, bool enable_hdr) {
		render_width_ = render_width;
		render_height_ = render_height;
		hdr_ = enable_hdr;

		glBindFramebuffer(GL_FRAMEBUFFER, main_fbo_);
		CreateTextures();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

} // namespace Boidsish
