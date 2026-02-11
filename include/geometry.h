#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Boidsish {

	/**
	 * @brief Contains all the data necessary for a single draw call.
	 * This decouples the what-to-render from the how-to-render.
	 */
	struct RenderPacket {
		unsigned int vao = 0;
		unsigned int vbo = 0;
		unsigned int ebo = 0;
		unsigned int shader_id = 0;

		unsigned int vertex_count = 0;
		unsigned int index_count = 0;

		// OpenGL drawing mode (e.g., GL_TRIANGLES)
		unsigned int draw_mode = 0;
		// OpenGL index type (e.g., GL_UNSIGNED_INT)
		unsigned int index_type = 0;

		// Transformation
		glm::mat4 model_matrix = glm::mat4(1.0f);

		// Material Properties
		glm::vec3 color = glm::vec3(1.0f);
		float     alpha = 1.0f;

		// PBR Properties
		bool  use_pbr = false;
		float roughness = 0.5f;
		float metallic = 0.0f;
		float ao = 1.0f;

		// Texture information
		struct TextureInfo {
			unsigned int id;
			std::string  type;
		};
		std::vector<TextureInfo> textures;

		// Instancing
		bool is_instanced = false;
		int  instance_count = 0;
	};

	/**
	 * @brief Abstract base class for geometric objects that can provide a RenderPacket.
	 * The ultimate goal is for geometry to return data needed to render it, and a
	 * different loop actually handles rendering.
	 */
	class Geometry {
	public:
		virtual ~Geometry() = default;

		/**
		 * @brief Generates a RenderPacket describing how this geometry should be rendered.
		 * @return A RenderPacket containing buffer IDs, shader IDs, and material properties.
		 */
		virtual RenderPacket render() const = 0;
	};

} // namespace Boidsish
