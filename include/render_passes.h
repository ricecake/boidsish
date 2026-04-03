#pragma once

#include <GL/glew.h>
#include <cstdint>
#include <functional>
#include <memory>

class Shader; // Global scope — external library type

// Forward-declare to keep this header lightweight and avoid include cascades
namespace Boidsish {
	struct FrameData;
	enum class RenderLayer : uint8_t;
} // namespace Boidsish

namespace Boidsish {

	class AkiraEffectManager;
	class DecorManager;
	class FireEffectManager;
	class HiZManager;
	class MeshExplosionManager;
	class NoiseManager;
	class SceneCompositor;
	class ShadowManager;
	class SdfVolumeManager;
	class TerrainRenderManager;

	namespace PostProcessing {
		class PostProcessingManager;
	}

	/**
	 * @brief Callbacks into VisualizerImpl rendering infrastructure.
	 *
	 * Built once per frame, captures `this` + FrameData. Allows render passes
	 * to submit draw calls and invoke helper methods without depending on
	 * VisualizerImpl directly. Temporary bridge until ExecuteRenderQueue and
	 * the terrain/sky/plane renderers are themselves extracted.
	 */
	struct RenderCallbacks {
		std::function<void(RenderLayer layer, bool dispatch_hiz)> execute_queue;
		std::function<void(Shader& s)>                            bind_shadows;
		std::function<void()>                                     update_frustum_ubo;
		std::function<void()>                                     render_terrain;
		std::function<void()>                                     render_plane;
		std::function<void()>                                     render_sky;
	};

	/**
	 * @brief Renders opaque geometry: decor, shapes, terrain, floor, sky.
	 */
	class OpaqueScenePass {
	public:
		OpaqueScenePass(
			DecorManager&                         decor,
			HiZManager&                           hiz,
			ShadowManager&                        shadows,
			Shader&                               main_shader,
			std::shared_ptr<TerrainRenderManager> terrain_render_manager
		);

		void
		Execute(const FrameData& frame, SceneCompositor& compositor, float render_scale, const RenderCallbacks& cb);

	private:
		DecorManager&                         decor_;
		HiZManager&                           hiz_;
		ShadowManager&                        shadows_;
		Shader&                               main_shader_;
		std::shared_ptr<TerrainRenderManager> terrain_render_manager_;
		bool                                  hiz_culling_enabled_ = true;
	};

	/**
	 * @brief Applies early post-processing (GTAO, etc.) between opaque and transparent.
	 */
	class EarlyEffectsPass {
	public:
		EarlyEffectsPass(PostProcessing::PostProcessingManager& post_fx, ShadowManager& shadows, Shader& main_shader);

		void Execute(const FrameData& frame, SceneCompositor& compositor);

	private:
		PostProcessing::PostProcessingManager& post_fx_;
		ShadowManager&                         shadows_;
		Shader&                                main_shader_;
	};

	/**
	 * @brief Renders fire, explosions, and other particle effects.
	 */
	class ParticleEffectsPass {
	public:
		ParticleEffectsPass(
			FireEffectManager&    fire,
			MeshExplosionManager& explosions,
			AkiraEffectManager*   akira,
			NoiseManager&         noise
		);

		void Execute(const FrameData& frame);

	private:
		FireEffectManager&    fire_;
		MeshExplosionManager& explosions_;
		AkiraEffectManager*   akira_;
		NoiseManager&         noise_;
	};

	/**
	 * @brief Renders SDF volumes and surfaces with temporal reprojection and depth guidance.
	 */
	class SdfVolumePass {
	public:
		SdfVolumePass(SdfVolumeManager& manager);
		~SdfVolumePass();

		// sceneTexture: current scene color (post early-effects)
		// depthTexture: scene depth buffer
		// targetFBO: FBO to composite the SDF result into
		void Execute(const FrameData& frame, GLuint sceneTexture, GLuint depthTexture, GLuint targetFBO);

	private:
		SdfVolumeManager& manager_;
		std::unique_ptr<Shader> shader_;
		GLuint history_textures_[2] = {0, 0};
		GLuint history_fbos_[2] = {0, 0};
		int width_ = 0, height_ = 0;
		int frame_index_ = 0;
		bool had_sources_ = false;

		void EnsureResources(int w, int h);
		void ClearHistory();
	};

	/**
	 * @brief Renders transparent geometry with proper blending.
	 */
	class TransparentPass {
	public:
		void Execute(const FrameData& frame, const RenderCallbacks& cb);
	};

} // namespace Boidsish
