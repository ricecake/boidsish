#pragma once

#include <memory>
#include <vector>

#include "shader.h"
#include "shape.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {
	class DecorManager;
	struct Camera;

	class SdfShadowRenderer {
	public:
		SdfShadowRenderer();
		~SdfShadowRenderer();

		void Initialize(int width, int height);
		void Resize(int width, int height);

		/**
		 * @brief Render screen-space SDF shadows for all relevant objects.
		 */
		void RenderShadows(
			const std::vector<std::shared_ptr<Shape>>& shapes,
			DecorManager*                              decor_manager,
			GLuint                                     depth_texture,
			GLuint                                     hiz_texture,
			const glm::mat4&                           view,
			const glm::mat4&                           projection,
			const glm::vec3&                           light_dir
		);

		GLuint GetShadowTexture() const { return shadow_texture_; }

	private:
		int width_ = 0, height_ = 0;
		GLuint fbo_ = 0;
		GLuint shadow_texture_ = 0;
		std::unique_ptr<Shader> collect_shader_;

		GLuint box_vao_ = 0;
		GLuint box_vbo_ = 0;
		GLuint box_ebo_ = 0;

		void _SetupBoxGeometry();
	};
} // namespace Boidsish
