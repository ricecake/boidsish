#include "render_passes.h"

#include "NoiseManager.h"
#include "akira_effect.h"
#include "decor_manager.h"
#include "fire_effect_manager.h"
#include "frame_data.h"
#include "geometry.h"
#include "hiz_manager.h"
#include "mesh_explosion_manager.h"
#include "post_processing/PostProcessingManager.h"
#include "profiler.h"
#include "scene_compositor.h"
#include "shadow_manager.h"
#include "sdf_volume_manager.h"
#include "terrain_render_manager.h"
#include <GL/glew.h>
#include <shader.h>

namespace Boidsish {

	// --- OpaqueScenePass ---

	OpaqueScenePass::OpaqueScenePass(
		DecorManager&                         decor,
		HiZManager&                           hiz,
		ShadowManager&                        shadows,
		Shader&                               main_shader,
		std::shared_ptr<TerrainRenderManager> terrain_render_manager
	):
		decor_(decor),
		hiz_(hiz),
		shadows_(shadows),
		main_shader_(main_shader),
		terrain_render_manager_(std::move(terrain_render_manager)) {}

	void OpaqueScenePass::Execute(
		const FrameData&       frame,
		SceneCompositor&       compositor,
		float                  render_scale,
		const RenderCallbacks& cb
	) {
		PROJECT_PROFILE_SCOPE("MainScenePass");
		compositor.BeginScene(frame, render_scale);

		cb.bind_shadows(main_shader_);
		cb.update_frustum_ubo();

		// Decor culling and rendering
		if (hiz_.IsInitialized() && hiz_culling_enabled_ && frame.frame_count > 0) {
			decor_.SetHiZData(
				hiz_.GetHiZTexture(),
				hiz_.GetWidth(),
				hiz_.GetHeight(),
				hiz_.GetMipCount(),
				frame.prev_view_projection
			);
		} else {
			decor_.SetHiZEnabled(false);
		}
		decor_.Cull(
			frame.view,
			frame.projection,
			frame.render_width,
			frame.render_height,
			std::nullopt,
			std::nullopt,
			terrain_render_manager_
		);
		decor_.Render(frame.view, frame.projection);

		cb.execute_queue(RenderLayer::Opaque, hiz_culling_enabled_ && frame.frame_count > 0);

		cb.render_terrain();
		cb.render_plane();
		cb.render_sky();
	}

	// --- EarlyEffectsPass ---

	EarlyEffectsPass::EarlyEffectsPass(
		PostProcessing::PostProcessingManager& post_fx,
		ShadowManager&                         shadows,
		Shader&                                main_shader
	):
		post_fx_(post_fx), shadows_(shadows), main_shader_(main_shader) {}

	void EarlyEffectsPass::Execute(const FrameData& frame, SceneCompositor& compositor) {
		if (!frame.config.effects_enabled)
			return;

		PROJECT_PROFILE_SCOPE("PostProcessing::Early");
		post_fx_.BeginApply(
			compositor.GetColorTexture(),
			compositor.GetMainFBO(),
			compositor.GetDepthTexture(),
			compositor.GetVelocityTexture()
		);
		post_fx_.ApplyEarlyEffects(frame.view, frame.projection, frame.camera_pos, frame.simulation_time);

		// Re-bind shadows after post-processing changed FBO state
		shadows_.GetShadowShader().use();
		main_shader_.use();
		// BindShadows needs the main shader active — the shadow UBO is already bound

		post_fx_.AttachDepthToCurrentFBO();
		glBindFramebuffer(GL_FRAMEBUFFER, post_fx_.GetCurrentFBO());
	}

	// --- ParticleEffectsPass ---

	ParticleEffectsPass::ParticleEffectsPass(
		FireEffectManager&    fire,
		MeshExplosionManager& explosions,
		AkiraEffectManager*   akira,
		NoiseManager&         noise
	):
		fire_(fire), explosions_(explosions), akira_(akira), noise_(noise) {}

	void ParticleEffectsPass::Execute(const FrameData& frame) {
		fire_.Render(
			frame.view,
			frame.projection,
			frame.camera_pos,
			noise_.GetNoiseTexture(),
			noise_.GetExtraNoiseTexture()
		);
		explosions_.Render(frame.view, frame.projection, frame.camera_pos);
		if (akira_) {
			akira_->Render(frame.view, frame.projection, frame.simulation_time);
		}
	}

	// --- SdfVolumePass ---

	SdfVolumePass::SdfVolumePass(SdfVolumeManager& manager): manager_(manager) {
		shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/sdf_volume.frag");
	}

	SdfVolumePass::~SdfVolumePass() {
		if (history_textures_[0] != 0) {
			glDeleteTextures(2, history_textures_);
			glDeleteFramebuffers(2, history_fbos_);
		}
	}

	void SdfVolumePass::EnsureResources(int w, int h) {
		if (width_ == w && height_ == h)
			return;

		if (history_textures_[0] != 0) {
			glDeleteTextures(2, history_textures_);
			glDeleteFramebuffers(2, history_fbos_);
		}

		width_ = w;
		height_ = h;

		glGenTextures(2, history_textures_);
		glGenFramebuffers(2, history_fbos_);

		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, history_textures_[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindFramebuffer(GL_FRAMEBUFFER, history_fbos_[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, history_textures_[i], 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void SdfVolumePass::ClearHistory() {
		if (history_fbos_[0] == 0)
			return;
		for (int i = 0; i < 2; ++i) {
			glBindFramebuffer(GL_FRAMEBUFFER, history_fbos_[i]);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		frame_index_ = 0;
	}

	void SdfVolumePass::Execute(const FrameData& frame, GLuint sceneTexture, GLuint depthTexture, GLuint targetFBO) {
		bool has_sources = manager_.GetSourceCount() > 0;

		if (!has_sources) {
			// Invalidate history when all sources disappear so we don't
			// get a stale flash when new sources appear later
			if (had_sources_) {
				ClearHistory();
			}
			had_sources_ = false;
			return;
		}

		PROJECT_PROFILE_SCOPE("SdfVolumePass");

		EnsureResources(frame.render_width, frame.render_height);

		// Clear history when sources first appear (or reappear after a gap)
		if (!had_sources_) {
			ClearHistory();
		}
		had_sources_ = true;

		int write_idx = frame_index_ % 2;
		int read_idx = (frame_index_ + 1) % 2;

		// --- Draw SDF scene into history FBO ---
		glBindFramebuffer(GL_FRAMEBUFFER, history_fbos_[write_idx]);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		shader_->use();
		shader_->setInt("sceneTexture", 0);
		shader_->setInt("depthTexture", 1);
		shader_->setInt("historyTexture", 2);
		shader_->setVec2("screenSize", glm::vec2(width_, height_));
		shader_->setVec3("cameraPos", frame.camera_pos);
		shader_->setMat4("invView", frame.inv_view);
		shader_->setMat4("invProjection", glm::inverse(frame.projection));
		shader_->setFloat("time", frame.simulation_time);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sceneTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthTexture);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, history_textures_[read_idx]);

		// Bind noise textures for rich volumetric detail
		if (noise_tex_) {
			shader_->trySetInt("u_noiseTexture", 5);
			shader_->trySetInt("u_curlTexture", 6);
			shader_->trySetInt("u_blueNoiseTexture", 7);
			shader_->trySetInt("u_extraNoiseTexture", 8);

			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_3D, noise_tex_);
			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_3D, curl_tex_);
			glActiveTexture(GL_TEXTURE7);
			glBindTexture(GL_TEXTURE_2D, blue_noise_tex_);
			glActiveTexture(GL_TEXTURE8);
			glBindTexture(GL_TEXTURE_3D, extra_noise_tex_);
		}

		manager_.BindSSBO(Constants::SsboBinding::SdfVolumes());

		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_BLEND);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		// --- Blit result to the target FBO (post-processing pipeline's current FBO) ---
		glBindFramebuffer(GL_READ_FRAMEBUFFER, history_fbos_[write_idx]);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Restore state: bind the target FBO and re-enable depth
		glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);

		frame_index_++;
	}

	// --- TransparentPass ---

	void TransparentPass::Execute(const FrameData& frame, const RenderCallbacks& cb) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);

		cb.execute_queue(RenderLayer::Transparent, false);

		glDepthMask(GL_TRUE);
	}

} // namespace Boidsish
