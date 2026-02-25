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

	/**
	 * @brief A rough polygon approximation of a slice of the model.
	 * Represented as a triangle soup for easy random point sampling.
	 */
	struct ModelSlice {
		std::vector<glm::vec3> triangles; // Triangle soup: 3 vertices per triangle

		/**
		 * @brief Returns a random point within the slice.
		 * @return A point in world space.
		 */
		glm::vec3 GetRandomPoint() const;
	};

	class Model: public Shape {
	public:
		// Constructor, expects a filepath to a 3D model.
		Model(const std::string& path, bool no_cull = false);

		void PrepareResources(Megabuffer* megabuffer = nullptr) const override;

		// Render the model
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;
		bool Intersects(const Ray& ray, float& t) const override;
		AABB GetAABB() const override;

		void GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const override;

		/**
		 * @brief Projects the given vector through the model and takes a perpendicular slice
		 * at a distance implied by the scale (0.0 to 1.0).
		 * @param direction The direction vector (in world space).
		 * @param scale The normalized distance along the model's extent in that direction.
		 * @return A ModelSlice containing a rough polygon approximation of the slice.
		 */
		ModelSlice GetSlice(const glm::vec3& direction, float scale) const;

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
