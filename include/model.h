#pragma once

#include <string>
#include <vector>

#include "shape.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	struct Texture {
		unsigned int id;
		uint64_t     handle = 0;
		std::string  type;
		std::string  path;
	};

	class Mesh {
	public:
		// Mesh Data
		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;
		std::vector<Texture>      textures;
		unsigned int              VAO;

		// Material Data
		glm::vec3 diffuseColor = glm::vec3(1.0f);
		float     opacity = 1.0f;

		// Constructor
		Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
		// ~Mesh();

		// Render the mesh
		void render() const;
		void render(Shader& shader) const; // Render with specific shader (for shadow pass, etc.)
		void render_instanced(int count, bool doVAO = true) const;

		// Bind textures for external rendering (e.g., instanced rendering with custom shaders)
		void bindTextures(Shader& shader) const;

	private:
		// Render data
		unsigned int VBO, EBO;

		// Initializes all the buffer objects/arrays
		void setupMesh();
	};

	struct ModelData {
		std::vector<Mesh>    meshes;
		std::vector<Texture> textures_loaded;
		std::string          directory;
		std::string          model_path;
		AABB                 aabb;

		// Unified buffers for MDI
		unsigned int unified_vao = 0;
		unsigned int unified_vbo = 0;
		unsigned int unified_ebo = 0;
		bool         has_unified = false;
	};

	class Model: public Shape {
	public:
		// Constructor, expects a filepath to a 3D model.
		Model(const std::string& path, bool no_cull = false);

		// Render the model
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		bool Intersects(const Ray& ray, float& t) const override;
		AABB GetAABB() const override;

		void GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const override;

		const std::vector<Mesh>& getMeshes() const;

		void SetBaseRotation(const glm::quat& rotation) { base_rotation_ = rotation; }

		void SetInstanced(bool is_instanced) { Shape::SetInstanced(is_instanced); }

		// Returns unique key for this model file - models loaded from the same file can be instanced together
		std::string GetInstanceKey() const override;

		// Get the model path
		const std::string& GetModelPath() const;

		// Exposed for AssetManager to fill during loading
		friend class AssetManager;

	private:
		// Model data
		glm::quat                  base_rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		std::shared_ptr<ModelData> m_data;
		bool                       no_cull_ = false;

		static unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma = false);
	};

} // namespace Boidsish
