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

	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
	};

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

		// Constructor
		Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);

		// Render the mesh
		void render() const;

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
		void render() const override;

		const std::vector<Mesh>& getMeshes() const { return meshes; }

	private:
		// Model data
		std::vector<Mesh>    meshes;
		std::string          directory;
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
