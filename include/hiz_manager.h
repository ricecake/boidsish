#pragma once

#include <memory>

#include <GL/glew.h>

class ComputeShader;

namespace Boidsish {

	/**
	 * @brief Manages a Hi-Z (Hierarchical Z-buffer) pyramid for GPU occlusion culling.
	 *
	 * Generates a mip chain from the previous frame's depth buffer where each mip level
	 * stores the MAX depth of its 2x2 source texels. This creates a conservative depth
	 * representation that can be tested against object AABBs to determine occlusion.
	 */
	class HiZManager {
	public:
		HiZManager();
		~HiZManager();

		/// Create the Hi-Z texture and compile compute shader.
		/// Call once after the main FBO is created.
		void Initialize(int width, int height);

		/// Recreate the Hi-Z texture when render resolution changes.
		void Resize(int width, int height);

		/// Generate the Hi-Z mip chain from the given depth texture.
		/// Call at the START of each frame, before occlusion culling.
		/// The depth texture should contain the previous frame's depth data.
		/// @param depthTexture The main FBO depth texture (GL_DEPTH24_STENCIL8 or GL_DEPTH_COMPONENT)
		void GeneratePyramid(GLuint depthTexture);

		GLuint GetHiZTexture() const { return hiz_texture_; }

		int GetMipCount() const { return mip_count_; }

		int GetWidth() const { return hiz_width_; }

		int GetHeight() const { return hiz_height_; }

		bool IsInitialized() const { return initialized_; }

	private:
		void CreateTexture();
		void DestroyTexture();

		std::unique_ptr<ComputeShader> generate_shader_;
		GLuint                         hiz_texture_ = 0;
		int                            render_width_ = 0; // Full render resolution (depth buffer size)
		int                            render_height_ = 0;
		int                            hiz_width_ = 0; // Hi-Z base resolution (half render res)
		int                            hiz_height_ = 0;
		int                            mip_count_ = 0;
		bool                           initialized_ = false;
	};

} // namespace Boidsish
