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

	class Model: public Shape {
	public:
		// Constructor, expects a filepath to a 3D model.
		Model(const std::string& path, bool no_cull = false);

		// Render the model
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		void GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const override;

		const std::vector<Mesh>& getMeshes() const { return meshes; }

		void SetBaseRotation(const glm::quat& rotation) { base_rotation_ = rotation; }

		void SetInstanced(bool is_instanced) { Shape::SetInstanced(is_instanced); }

		// Returns unique key for this model file - models loaded from the same file can be instanced together
		std::string GetInstanceKey() const override { return "Model:" + model_path_; }

		// Get the model path
		const std::string& GetModelPath() const { return model_path_; }

	private:
		// Model data
		glm::quat            base_rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		std::vector<Mesh>    meshes;
		std::string          directory;
		std::string          model_path_; // Full path to the model file for instancing identification
		bool                 no_cull_ = false;
		std::vector<Texture> textures_loaded_;

		// Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in meshes vector.
		void loadModel(const std::string& path);

		// Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this
		// process on its children nodes (if any).
		void                 processNode(aiNode* node, const aiScene* scene);
		Mesh                 processMesh(aiMesh* mesh, const aiScene* scene);
		std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
		static unsigned int  TextureFromFile(const char* path, const std::string& directory, bool gamma = false);
	};

} // namespace Boidsish
