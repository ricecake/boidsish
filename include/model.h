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
		std::string  type;
		std::string  path;
	};

	class Mesh {
	public:
		// Mesh Data
		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;
		std::vector<Texture>      textures;

		// Material Data
		glm::vec3 diffuseColor = glm::vec3(1.0f);
		float     opacity = 1.0f;

		// Constructor
		Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);

		// Render the mesh
		void render() const;
		void render(Shader& shader) const; // Render with specific shader (for shadow pass, etc.)
		void render_instanced(int count, bool doVAO = true) const;

		// Bind textures for external rendering (e.g., instanced rendering with custom shaders)
		void bindTextures(Shader& shader) const;

		unsigned int getVAO() const { return VAO; }

		unsigned int getVBO() const { return VBO; }

		unsigned int getEBO() const { return EBO; }

		MegabufferAllocation allocation;

	private:
		// Render data
		unsigned int VAO = 0, VBO = 0, EBO = 0;

		// Initializes all the buffer objects/arrays
		void setupMesh(Megabuffer* megabuffer = nullptr);

		friend class Model;
	};

	struct ModelData {
		std::vector<Mesh>    meshes;
		std::vector<Texture> textures_loaded;
		std::string          directory;
		std::string          model_path;
		AABB                 aabb;
	};

	class Model: public Shape {
	public:
		// Constructor, expects a filepath to a 3D model.
		Model(const std::string& path, bool no_cull = false);

		// Constructor for programmatically created model data.
		Model(std::shared_ptr<ModelData> data, bool no_cull = false);

		void PrepareResources(Megabuffer* megabuffer = nullptr) const override;

		// Render the model
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;
		bool Intersects(const Ray& ray, float& t) const override;
		AABB GetAABB() const override;

		void GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const override;

		const std::vector<Mesh>& getMeshes() const;

		void SetBaseRotation(const glm::quat& rotation) { base_rotation_ = rotation; }

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
