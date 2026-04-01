#pragma once

#include <memory>

#include "frame_data.h"
#include <GL/glew.h>

class Shader;

namespace Boidsish {

	class ShockwaveManager;

	namespace PostProcessing {
		class PostProcessingManager;
	}

	/**
	 * @brief Owns the intermediate FBOs for the scene render pipeline.
	 *
	 * Provides BeginScene/CaptureRefraction/ResolveToScreen for bracketing
	 * the opaque → transparent → composite sequence. Extracted from VisualizerImpl
	 * so render passes don't need to know about FBO internals.
	 */
	class SceneCompositor {
	public:
		SceneCompositor(int render_width, int render_height, bool enable_hdr, GLuint blit_vao);
		~SceneCompositor();

		// Non-copyable
		SceneCompositor(const SceneCompositor&) = delete;
		SceneCompositor& operator=(const SceneCompositor&) = delete;

		/// Bind the appropriate FBO for the opaque pass. Clears color+depth.
		void BeginScene(const FrameData& frame, float render_scale);

		/// Copy current framebuffer color to the refraction texture.
		void CaptureRefraction(const FrameData& frame, bool effects_enabled, GLuint post_fx_fbo);

		/// Blit/resolve the final image to the screen backbuffer.
		void ResolveToScreen(
			const FrameData&                       frame,
			float                                  render_scale,
			PostProcessing::PostProcessingManager* post_fx,
			ShockwaveManager*                      shockwave
		);

		void Resize(int render_width, int render_height, bool enable_hdr);

		// --- Accessors for external consumers ---
		GLuint GetMainFBO() const { return main_fbo_; }

		GLuint GetColorTexture() const { return color_tex_; }

		GLuint GetDepthTexture() const { return depth_tex_; }

		GLuint GetVelocityTexture() const { return velocity_tex_; }

		GLuint GetRefractionTexture() const { return refraction_tex_; }

		GLuint GetBlitVAO() const { return blit_vao_; }

	private:
		GLuint main_fbo_ = 0;
		GLuint color_tex_ = 0;
		GLuint velocity_tex_ = 0;
		GLuint depth_tex_ = 0;
		GLuint refraction_tex_ = 0;

		// Blit quad — owned externally (shared with post-processing), just referenced
		GLuint                  blit_vao_ = 0;
		std::shared_ptr<Shader> blit_shader_;

		int  render_width_ = 0;
		int  render_height_ = 0;
		bool hdr_ = true;

		void CreateTextures();
	};

} // namespace Boidsish
