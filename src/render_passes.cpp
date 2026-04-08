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
			compositor.GetVelocityTexture(),
			compositor.GetNormalTexture()
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

	// --- TransparentPass ---

	void TransparentPass::Execute(const FrameData& frame, const RenderCallbacks& cb) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);

		cb.execute_queue(RenderLayer::Transparent, false);

		glDepthMask(GL_TRUE);
	}

} // namespace Boidsish
