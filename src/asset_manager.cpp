#include "asset_manager.h"

#include <algorithm>
#include <iostream>

#include "ConfigManager.h"
#include "logger.h"
#include "mesh_optimizer_util.h"
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

	// Helper for Assimp processing (moved from Model class)
	namespace {
		std::vector<Texture> LoadMaterialTextures(
			aiMaterial*        mat,
			aiTextureType      type,
			std::string        typeName,
			ModelData&         data,
			const std::string& directory,
			const aiScene*     scene
		) {
			std::vector<Texture> textures;
			for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
				aiString str;
				mat->GetTexture(type, i, &str);

				// Handle embedded textures
				const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());
				if (!embeddedTexture) {
					// Fallback: try filename only for embedded textures
					std::string texture_path = str.C_Str();
					size_t last_sep = texture_path.find_last_of("/\\");
					std::string filename = (last_sep != std::string::npos) ? texture_path.substr(last_sep + 1) : texture_path;
					if (filename != texture_path) {
						embeddedTexture = scene->GetEmbeddedTexture(filename.c_str());
					}
				}

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
					if (embeddedTexture) {
						// Load embedded texture
						int            width, height, nrComponents;
						unsigned char* imageData = nullptr;
						if (embeddedTexture->mHeight == 0) {
							// Compressed texture (e.g. png, jpg)
							// Force 4 components (RGBA) for consistency
							imageData = stbi_load_from_memory(
								reinterpret_cast<unsigned char*>(embeddedTexture->pcData),
								embeddedTexture->mWidth,
								&width,
								&height,
								&nrComponents,
								4
							);

							if (imageData) {
								glGenTextures(1, &texture.id);
								GLenum format = GL_RGBA;

								glBindTexture(GL_TEXTURE_2D, texture.id);
								glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
								glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, imageData);
								glGenerateMipmap(GL_TEXTURE_2D);

								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

								stbi_image_free(imageData);
								logger::LOG("Compressed embedded texture loaded: {}", str.C_Str());
							} else {
								logger::ERROR("Failed to load compressed embedded texture: {}", str.C_Str());
								texture.id = 0;
							}
						} else {
							// Uncompressed texture (Assimp uses ARGB8888 or BGRA8888)
							// Assimp documentation says: "Each pixel is stored in 32-bit (8 bits per channel)"
							// Most common is BGRA.
							glGenTextures(1, &texture.id);
							glBindTexture(GL_TEXTURE_2D, texture.id);
							glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
							glTexImage2D(
								GL_TEXTURE_2D,
								0,
								GL_RGBA,
								embeddedTexture->mWidth,
								embeddedTexture->mHeight,
								0,
								GL_BGRA,
								GL_UNSIGNED_BYTE,
								embeddedTexture->pcData
							);
							glGenerateMipmap(GL_TEXTURE_2D);

							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

							logger::LOG("Raw embedded texture loaded: {} ({}x{})", str.C_Str(), embeddedTexture->mWidth, embeddedTexture->mHeight);
						}
					} else {
						// Robust file-based texture lookup
						std::string texture_path = str.C_Str();
						texture.id = AssetManager::GetInstance().GetTexture(texture_path, directory);

						if (texture.id == 0) {
							// Try resolving filename only in model directory
							size_t last_sep = texture_path.find_last_of("/\\");
							std::string filename = (last_sep != std::string::npos) ? texture_path.substr(last_sep + 1) : texture_path;
							if (filename != texture_path) {
								logger::LOG("Texture failed to load, trying fallback: {}/{}", directory, filename);
								texture.id = AssetManager::GetInstance().GetTexture(filename, directory);
							}
						}
					}
					if (texture.id != 0) {
						texture.type = typeName;
						texture.path = str.C_Str();
						textures.push_back(texture);
						data.textures_loaded.push_back(texture);
					}
				}
			}
			return textures;
		}

		glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from) {
			glm::mat4 to;
			to[0][0] = from.a1;
			to[1][0] = from.a2;
			to[2][0] = from.a3;
			to[3][0] = from.a4;
			to[0][1] = from.b1;
			to[1][1] = from.b2;
			to[2][1] = from.b3;
			to[3][1] = from.b4;
			to[0][2] = from.c1;
			to[1][2] = from.c2;
			to[2][2] = from.c3;
			to[3][2] = from.c4;
			to[0][3] = from.d1;
			to[1][3] = from.d2;
			to[2][3] = from.d3;
			to[3][3] = from.d4;
			return to;
		}

		glm::vec3 GetGLMVec(const aiVector3D& vec) {
			return glm::vec3(vec.x, vec.y, vec.z);
		}

		glm::quat GetGLMQuat(const aiQuaternion& pOrientation) {
			return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
		}

		void SetVertexBoneDataToDefault(Vertex& vertex) {
			for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
				vertex.m_BoneIDs[i] = -1;
				vertex.m_Weights[i] = 0.0f;
			}
		}

		void SetVertexBoneData(Vertex& vertex, int boneID, float weight) {
			for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
				if (vertex.m_BoneIDs[i] < 0) {
					vertex.m_Weights[i] = weight;
					vertex.m_BoneIDs[i] = boneID;
					break;
				}
			}
		}

		Mesh ProcessMesh(
			aiMesh*            mesh,
			const aiScene*     scene,
			ModelData&         data,
			const std::string& directory,
			const std::string& nodeName,
			const glm::mat4&   globalTransform
		);

		void ExtractBoneWeightForVertices(
			std::vector<Vertex>& vertices,
			aiMesh*              mesh,
			ModelData&           data,
			const std::string&   nodeName,
			const glm::mat4&     globalTransform
		) {
			if (mesh->mNumBones == 0) {
				// Mesh has no bones - assign to the node itself to follow hierarchy
				int boneID = -1;
				if (data.bone_info_map.find(nodeName) == data.bone_info_map.end()) {
					BoneInfo newBoneInfo;
					newBoneInfo.id = data.bone_count;
					// Virtual bone offset must account for pre-transformed vertices
					newBoneInfo.offset = glm::inverse(globalTransform);
					data.bone_info_map[nodeName] = newBoneInfo;
					boneID = data.bone_count;
					data.bone_count++;
				} else {
					boneID = data.bone_info_map[nodeName].id;
				}
				for (auto& vertex : vertices) {
					SetVertexBoneData(vertex, boneID, 1.0f);
				}
				return;
			}

			for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
				int         boneID = -1;
				std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
				glm::mat4   offset = ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);

				if (data.bone_info_map.find(boneName) == data.bone_info_map.end()) {
					BoneInfo newBoneInfo;
					newBoneInfo.id = data.bone_count;
					// Adjusted offset to account for pre-transformed vertices in root space
					newBoneInfo.offset = offset * glm::inverse(globalTransform);
					data.bone_info_map[boneName] = newBoneInfo;
					boneID = data.bone_count;
					data.bone_count++;
				} else {
					boneID = data.bone_info_map[boneName].id;
				}
				assert(boneID != -1);
				auto weights = mesh->mBones[boneIndex]->mWeights;
				int  numWeights = mesh->mBones[boneIndex]->mNumWeights;

				for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex) {
					int   vertexId = weights[weightIndex].mVertexId;
					float weight = weights[weightIndex].mWeight;
					assert(vertexId < (int)vertices.size());
					SetVertexBoneData(vertices[vertexId], boneID, weight);
				}
			}

			// Normalize weights to 1.0
			for (auto& vertex : vertices) {
				float totalWeight = 0.0f;
				for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
					if (vertex.m_BoneIDs[i] != -1)
						totalWeight += vertex.m_Weights[i];
				}
				if (totalWeight > 0.0f) {
					for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
						vertex.m_Weights[i] /= totalWeight;
					}
				}
			}
		}

		Mesh ProcessMesh(
			aiMesh*            mesh,
			const aiScene*     scene,
			ModelData&         data,
			const std::string& directory,
			const std::string& nodeName,
			const glm::mat4&   globalTransform
		) {
			std::vector<Vertex>       vertices;
			std::vector<unsigned int> indices;
			std::vector<Texture>      textures;

			for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
				Vertex    vertex;
				SetVertexBoneDataToDefault(vertex);

				// Pre-transform vertices to root space
				glm::vec4 pos = globalTransform * glm::vec4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
				vertex.Position = glm::vec3(pos);

				if (mesh->HasNormals()) {
					// Pre-transform normals to root space
					glm::vec3 normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
					vertex.Normal = normalize(glm::mat3(transpose(inverse(globalTransform))) * normal);
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

			ExtractBoneWeightForVertices(vertices, mesh, data, nodeName, globalTransform);

			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
				aiFace face = mesh->mFaces[i];
				for (unsigned int j = 0; j < face.mNumIndices; j++)
					indices.push_back(face.mIndices[j]);
			}

			// Apply mesh optimization/simplification before finalizing the mesh
			auto& config = ConfigManager::GetInstance();
			if (config.GetAppSettingBool("mesh_simplifier_enabled", false)) {
				float error = config.GetAppSettingFloat("mesh_simplifier_error_prebuild", 0.01f);
				float ratio = config.GetAppSettingFloat("mesh_simplifier_target_ratio", 0.5f);
				int   flags = config.GetAppSettingInt("mesh_simplifier_aggression_prebuild", 0);
				MeshOptimizerUtil::Simplify(vertices, indices, error, ratio, (unsigned int)flags, data.model_path);
			}
			std::vector<unsigned int> shadow_indices;
			if (config.GetAppSettingBool("mesh_optimizer_enabled", true)) {
				MeshOptimizerUtil::Optimize(vertices, indices, data.model_path);

				if (config.GetAppSettingBool("mesh_optimizer_shadow_indices_enabled", true)) {
					MeshOptimizerUtil::GenerateShadowIndices(vertices, indices, shadow_indices);
				}
			}

			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			std::vector<Texture> diffuseMaps =
				LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", data, directory, scene);
			if (diffuseMaps.empty()) {
				// Fallback for some models (e.g. GLTF/OBJ sometimes use BASE_COLOR or AMBIENT as diffuse)
				diffuseMaps = LoadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse", data, directory, scene);
				if (diffuseMaps.empty()) {
					diffuseMaps = LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_diffuse", data, directory, scene);
				}
			}
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
			std::vector<Texture> specularMaps =
				LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", data, directory, scene);
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
			std::vector<Texture> normalMaps =
				LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", data, directory, scene);
			textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
			std::vector<Texture> heightMaps =
				LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height", data, directory, scene);
			textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

			Mesh out_mesh(vertices, indices, textures, shadow_indices);

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

		void ProcessNode(
			aiNode*            node,
			const aiScene*     scene,
			ModelData&         data,
			const std::string& directory,
			const glm::mat4&   parentTransform = glm::mat4(1.0f)
		) {
			std::string nodeName = node->mName.C_Str();
			glm::mat4   nodeTransform = ConvertMatrixToGLMFormat(node->mTransformation);
			glm::mat4   globalTransform = parentTransform * nodeTransform;

			for (unsigned int i = 0; i < node->mNumMeshes; i++) {
				aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
				data.meshes.push_back(ProcessMesh(mesh, scene, data, directory, nodeName, globalTransform));
			}
			for (unsigned int i = 0; i < node->mNumChildren; i++) {
				ProcessNode(node->mChildren[i], scene, data, directory, globalTransform);
			}
		}

		void ReadHierarchyData(NodeData& dest, const aiNode* src) {
			assert(src);

			dest.name = src->mName.data;
			dest.transformation = ConvertMatrixToGLMFormat(src->mTransformation);
			dest.childrenCount = src->mNumChildren;

			for (unsigned int i = 0; i < src->mNumChildren; i++) {
				NodeData newData;
				ReadHierarchyData(newData, src->mChildren[i]);
				dest.children.push_back(newData);
			}
		}

		void ReadAnimations(const aiScene* scene, ModelData& data) {
			for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
				aiAnimation* aiAnim = scene->mAnimations[i];
				Animation    anim;
				anim.name = aiAnim->mName.C_Str();
				anim.duration = (float)aiAnim->mDuration;
				anim.ticksPerSecond = (int)aiAnim->mTicksPerSecond;

				for (unsigned int j = 0; j < aiAnim->mNumChannels; j++) {
					aiNodeAnim*   channel = aiAnim->mChannels[j];
					BoneAnimation boneAnim;
					boneAnim.name = channel->mNodeName.data;

					boneAnim.numPositions = channel->mNumPositionKeys;
					for (unsigned int k = 0; k < boneAnim.numPositions; k++) {
						aiVector3D  aiVec = channel->mPositionKeys[k].mValue;
						float       timeStamp = (float)channel->mPositionKeys[k].mTime;
						KeyPosition data_pos;
						data_pos.position = GetGLMVec(aiVec);
						data_pos.timeStamp = timeStamp;
						boneAnim.positions.push_back(data_pos);
					}

					boneAnim.numRotations = channel->mNumRotationKeys;
					for (unsigned int k = 0; k < boneAnim.numRotations; k++) {
						aiQuaternion aiQuat = channel->mRotationKeys[k].mValue;
						float        timeStamp = (float)channel->mRotationKeys[k].mTime;
						KeyRotation  data_rot;
						data_rot.orientation = GetGLMQuat(aiQuat);
						data_rot.timeStamp = timeStamp;
						boneAnim.rotations.push_back(data_rot);
					}

					boneAnim.numScalings = channel->mNumScalingKeys;
					for (unsigned int k = 0; k < boneAnim.numScalings; k++) {
						aiVector3D aiVec = channel->mScalingKeys[k].mValue;
						float      timeStamp = (float)channel->mScalingKeys[k].mTime;
						KeyScale   data_scale;
						data_scale.scale = GetGLMVec(aiVec);
						data_scale.timeStamp = timeStamp;
						boneAnim.scales.push_back(data_scale);
					}
					anim.boneAnimations.push_back(boneAnim);
				}
				data.animations.push_back(anim);
			}
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
			aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_LimitBoneWeights
		);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			logger::ERROR("ASSIMP Error: {}", importer.GetErrorString());
			return nullptr;
		}

		auto data = std::make_shared<ModelData>();
		data->model_path = path;
		logger::LOG("Attempting to load model: {}", path);
		size_t last_slash = path.find_last_of("/\\");
		if (last_slash != std::string::npos) {
			data->directory = path.substr(0, last_slash);
		} else {
			data->directory = ".";
		}

		data->global_inverse_transform = glm::mat4(1.0f); // Pre-transformed to root, so identity is fine

		ProcessNode(scene->mRootNode, scene, *data, data->directory);
		ReadHierarchyData(data->root_node, scene->mRootNode);
		ReadAnimations(scene, *data);

		logger::LOG("Model loaded: {} bones, {} animations", data->bone_count, data->animations.size());

		// Calculate AABB from pre-transformed meshes
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
