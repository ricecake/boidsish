#include "asset_manager.h"

#include <algorithm>
#include <iostream>

#include "logger.h"
#include "miniaudio.h"
#include "model.h"
#include "stb_image.h"
#include <GL/glew.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Boidsish {

	AssetManager& AssetManager::GetInstance() {
		static AssetManager instance;
		return instance;
	}

	AssetManager::~AssetManager() {
		Clear();
	}

	void AssetManager::Clear() {
		for (auto& [path, textureId] : m_textures) {
			glDeleteTextures(1, &textureId);
		}
		m_textures.clear();
		m_models.clear();
		m_audio_sources.clear();
	}

	std::vector<std::shared_ptr<ModelData>> AssetManager::GetAllModels() {
		std::vector<std::shared_ptr<ModelData>> models;
		for (auto& [path, data] : m_models) {
			models.push_back(data);
		}
		return models;
	}

	// Helper for Assimp processing (moved from Model class)
	namespace {
		void ProcessNode(aiNode* node, const aiScene* scene, ModelData& data, const std::string& directory);
		Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, ModelData& data, const std::string& directory);
		std::vector<Texture> LoadMaterialTextures(
			aiMaterial*        mat,
			aiTextureType      type,
			std::string        typeName,
			ModelData&         data,
			const std::string& directory
		);

		void ProcessNode(aiNode* node, const aiScene* scene, ModelData& data, const std::string& directory) {
			for (unsigned int i = 0; i < node->mNumMeshes; i++) {
				aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
				data.meshes.push_back(ProcessMesh(mesh, scene, data, directory));
			}
			for (unsigned int i = 0; i < node->mNumChildren; i++) {
				ProcessNode(node->mChildren[i], scene, data, directory);
			}
		}

		Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, ModelData& data, const std::string& directory) {
			std::vector<Vertex>       vertices;
			std::vector<unsigned int> indices;
			std::vector<Texture>      textures;

			for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
				Vertex    vertex;
				glm::vec3 vector;
				vector.x = mesh->mVertices[i].x;
				vector.y = mesh->mVertices[i].y;
				vector.z = mesh->mVertices[i].z;
				vertex.Position = vector;
				if (mesh->HasNormals()) {
					vector.x = mesh->mNormals[i].x;
					vector.y = mesh->mNormals[i].y;
					vector.z = mesh->mNormals[i].z;
					vertex.Normal = vector;
				}
				if (mesh->mTextureCoords[0]) {
					glm::vec2 vec;
					vec.x = mesh->mTextureCoords[0][i].x;
					vec.y = mesh->mTextureCoords[0][i].y;
					vertex.TexCoords = vec;
				} else {
					vertex.TexCoords = glm::vec2(0.0f, 0.0f);
				}
				vertices.push_back(vertex);
			}
			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
				aiFace face = mesh->mFaces[i];
				for (unsigned int j = 0; j < face.mNumIndices; j++)
					indices.push_back(face.mIndices[j]);
			}

			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			std::vector<Texture> diffuseMaps =
				LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", data, directory);
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
			std::vector<Texture> specularMaps =
				LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", data, directory);
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
			std::vector<Texture> normalMaps =
				LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", data, directory);
			textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
			std::vector<Texture> heightMaps =
				LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height", data, directory);
			textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

			Mesh out_mesh(vertices, indices, textures);

			aiColor3D color(1.0f, 1.0f, 1.0f);
			if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
				out_mesh.diffuseColor = glm::vec3(color.r, color.g, color.b);
			}

			float opacity = 1.0f;
			if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
				out_mesh.opacity = opacity;
			}

			return out_mesh;
		}

		std::vector<Texture> LoadMaterialTextures(
			aiMaterial*        mat,
			aiTextureType      type,
			std::string        typeName,
			ModelData&         data,
			const std::string& directory
		) {
			std::vector<Texture> textures;
			for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
				aiString str;
				mat->GetTexture(type, i, &str);
				bool skip = false;
				for (unsigned int j = 0; j < data.textures_loaded.size(); j++) {
					if (std::strcmp(data.textures_loaded[j].path.data(), str.C_Str()) == 0) {
						textures.push_back(data.textures_loaded[j]);
						skip = true;
						break;
					}
				}
				if (!skip) {
					Texture texture;
					texture.id = AssetManager::GetInstance().GetTexture(str.C_Str(), directory);
					texture.type = typeName;
					texture.path = str.C_Str();
					textures.push_back(texture);
					data.textures_loaded.push_back(texture);
				}
			}
			return textures;
		}
	} // anonymous namespace

	std::shared_ptr<ModelData> AssetManager::GetModelData(const std::string& path) {
		auto it = m_models.find(path);
		if (it != m_models.end()) {
			return it->second;
		}

		logger::LOG("Model loading from disk: {}", path);
		Assimp::Importer importer;
		const aiScene*   scene = importer.ReadFile(
            path,
            aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
        );

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			logger::ERROR("ASSIMP Error: {}", importer.GetErrorString());
			return nullptr;
		}

		auto data = std::make_shared<ModelData>();
		data->model_path = path;
		size_t last_slash = path.find_last_of("/\\");
		if (last_slash != std::string::npos) {
			data->directory = path.substr(0, last_slash);
		} else {
			data->directory = ".";
		}

		ProcessNode(scene->mRootNode, scene, *data, data->directory);

		// Calculate AABB
		glm::vec3 min(std::numeric_limits<float>::max());
		glm::vec3 max(-std::numeric_limits<float>::max());
		bool      has_vertices = false;
		for (const auto& mesh : data->meshes) {
			for (const auto& vertex : mesh.vertices) {
				min = glm::min(min, vertex.Position);
				max = glm::max(max, vertex.Position);
				has_vertices = true;
			}
		}
		if (has_vertices) {
			data->aabb = AABB(min, max);
		} else {
			data->aabb = AABB(glm::vec3(0.0f), glm::vec3(0.0f));
		}

		logger::LOG("Model cached: {} with {} meshes", path, data->meshes.size());
		m_models[path] = data;
		return data;
	}

	GLuint AssetManager::GetTexture(const std::string& path, const std::string& directory) {
		std::string filename = path;
		std::replace(filename.begin(), filename.end(), '\\', '/');

		std::string fullPath;
		if (!directory.empty()) {
			fullPath = directory + '/' + filename;
		} else {
			fullPath = filename;
		}

		auto it = m_textures.find(fullPath);
		if (it != m_textures.end()) {
			return it->second;
		}

		unsigned int textureID;
		glGenTextures(1, &textureID);

		int            width, height, nrComponents;
		unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &nrComponents, 0);
		if (data) {
			GLenum format = GL_RGB;
			if (nrComponents == 1)
				format = GL_RED;
			else if (nrComponents == 3)
				format = GL_RGB;
			else if (nrComponents == 4)
				format = GL_RGBA;

			glBindTexture(GL_TEXTURE_2D, textureID);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			stbi_image_free(data);
			logger::LOG("Texture loaded and cached: {}", fullPath);
			m_textures[fullPath] = textureID;
		} else {
			logger::ERROR("Texture failed to load at path: {}", fullPath);
			stbi_image_free(data);
			glDeleteTextures(1, &textureID);
			return 0;
		}

		return textureID;
	}

	std::shared_ptr<ma_resource_manager_data_source>
	AssetManager::GetAudioDataSource(const std::string& path, ma_engine* engine) {
		auto it = m_audio_sources.find(path);
		if (it != m_audio_sources.end()) {
			return it->second;
		}

		if (!engine) {
			logger::ERROR("Cannot load audio data source: NULL engine for path {}", path);
			return nullptr;
		}

		auto pResourceManager = ma_engine_get_resource_manager(engine);
		if (!pResourceManager) {
			logger::ERROR("Engine has no resource manager for path {}", path);
			return nullptr;
		}

		auto dataSource = std::shared_ptr<ma_resource_manager_data_source>(
			new ma_resource_manager_data_source,
			[](ma_resource_manager_data_source* p) {
				ma_resource_manager_data_source_uninit(p);
				delete p;
			}
		);

		ma_result result =
			ma_resource_manager_data_source_init(pResourceManager, path.c_str(), 0, NULL, dataSource.get());
		if (result != MA_SUCCESS) {
			logger::ERROR("Failed to load audio data source: {}", path);
			return nullptr;
		}

		logger::LOG("Audio data source cached: {}", path);
		m_audio_sources[path] = dataSource;
		return dataSource;
	}

} // namespace Boidsish
