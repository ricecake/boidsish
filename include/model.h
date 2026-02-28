#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "shape.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	class Animator;

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
		std::vector<unsigned int> shadow_indices;
		std::vector<Texture>      textures;

		// Material Data
		glm::vec3 diffuseColor = glm::vec3(1.0f);
		float     opacity = 1.0f;
		bool      has_vertex_colors = false;

		// Constructor
		Mesh(
			std::vector<Vertex>       vertices,
			std::vector<unsigned int> indices,
			std::vector<Texture>      textures,
			std::vector<unsigned int> shadow_indices = {}
		);

		// Render the mesh
		void render() const;
		void render(Shader& shader) const; // Render with specific shader (for shadow pass, etc.)
		void render_instanced(int count, bool doVAO = true) const;

		// Bind textures for external rendering (e.g., instanced rendering with custom shaders)
		void bindTextures(Shader& shader) const;

		void Cleanup();
		void UploadToGPU();

		unsigned int getVAO() const { return VAO; }

		unsigned int getVBO() const { return VBO; }

		unsigned int getEBO() const { return EBO; }

		unsigned int getShadowEBO() const { return shadow_EBO; }

		MegabufferAllocation allocation;
		MegabufferAllocation shadow_allocation;

	private:
		// Render data
		unsigned int VAO = 0, VBO = 0, EBO = 0, shadow_EBO = 0;

		// Initializes all the buffer objects/arrays
		void setupMesh(Megabuffer* megabuffer = nullptr);

		friend class Model;
	};

	enum class ConstraintType {
		None,
		Hinge,
		Cone
	};

	struct BoneConstraint {
		ConstraintType type = ConstraintType::None;
		glm::vec3      axis = glm::vec3(1, 0, 0); // For Hinge
		float          minAngle = -180.0f;        // In degrees
		float          maxAngle = 180.0f;
		float          coneAngle = 45.0f;         // For Cone
	};

	struct BoneInfo {
		/*id is index in finalBoneMatrices*/
		int id;

		/*offset matrix transforms vertex from model space to bone space*/
		glm::mat4 offset;

		BoneConstraint constraint;
	};

	struct KeyPosition {
		glm::vec3 position;
		float     timeStamp;
	};

	struct KeyRotation {
		glm::quat orientation;
		float     timeStamp;
	};

	struct KeyScale {
		glm::vec3 scale;
		float     timeStamp;
	};

	struct BoneAnimation {
		std::vector<KeyPosition> positions;
		std::vector<KeyRotation> rotations;
		std::vector<KeyScale>    scales;
		int                      numPositions;
		int                      numRotations;
		int                      numScalings;

		glm::mat4   localTransform;
		std::string name;
		int         id;
	};

	struct NodeData {
		glm::mat4             transformation = glm::mat4(1.0f);
		std::string           name;
		int                   childrenCount = 0;
		std::vector<NodeData> children;

		NodeData* FindNode(const std::string& name) {
			if (this->name == name)
				return this;
			for (auto& child : children) {
				NodeData* found = child.FindNode(name);
				if (found)
					return found;
			}
			return nullptr;
		}

		const NodeData* FindNode(const std::string& name) const {
			if (this->name == name)
				return this;
			for (auto& child : children) {
				const NodeData* found = child.FindNode(name);
				if (found)
					return found;
			}
			return nullptr;
		}
	};

	struct Animation {
		float                      duration;
		int                        ticksPerSecond;
		std::vector<BoneAnimation> boneAnimations;
		std::string                name;
	};

	struct ModelData {
		std::vector<Mesh>    meshes;
		std::vector<Texture> textures_loaded;
		std::string          directory;
		std::string          model_path;
		AABB                 aabb;

		// Animation Data
		std::map<std::string, BoneInfo> bone_info_map;
		int                             bone_count = 0;
		glm::mat4                       global_inverse_transform = glm::mat4(1.0f);
		std::vector<Animation>          animations;
		NodeData                        root_node;

		void AddBone(const std::string& name, const std::string& parentName, const glm::mat4& localTransform) {
			if (bone_info_map.find(name) != bone_info_map.end())
				return;

			NodeData* parentNode = root_node.FindNode(parentName);
			if (!parentNode && !parentName.empty())
				return;

			NodeData newNode;
			newNode.name = name;
			newNode.transformation = localTransform;
			newNode.childrenCount = 0;

			if (parentNode) {
				parentNode->children.push_back(newNode);
				parentNode->childrenCount++;
			} else {
				// If no parent, it's either root or should be attached to root
				root_node.children.push_back(newNode);
				root_node.childrenCount++;
			}

			BoneInfo info;
			info.id = bone_count++;

			// Calculate offset matrix (inverse of global bind transform)
			glm::mat4 parentGlobal = glm::mat4(1.0f);
			if (!parentName.empty()) {
				std::function<bool(const NodeData&, glm::mat4, glm::mat4&)> findGlobal =
					[&](const NodeData& n, glm::mat4 p, glm::mat4& out) -> bool {
					glm::mat4 g = p * n.transformation;
					if (n.name == parentName) {
						out = g;
						return true;
					}
					for (const auto& c : n.children) {
						if (findGlobal(c, g, out))
							return true;
					}
					return false;
				};
				findGlobal(root_node, glm::mat4(1.0f), parentGlobal);
			}
			info.offset = glm::inverse(parentGlobal * localTransform);
			bone_info_map[name] = info;
		}
	};

	/**
	 * @brief A rough polygon approximation of a slice of the model.
	 * Represented as a triangle soup for easy random point sampling.
	 */
	struct ModelSlice {
		std::vector<glm::vec3> triangles; // Triangle soup: 3 vertices per triangle
		float                  area = 0.0f;

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

		// Constructor for programmatically created model data.
		Model(std::shared_ptr<ModelData> data, bool no_cull = false);

		~Model();

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

		/**
		 * @brief Set dissolve using a normalized sweep value (0.0 to 1.0)
		 * which is automatically mapped to the model's extent in the given direction.
		 */
		void SetDissolveSweep(const glm::vec3& direction, float sweep);

		// Get the model path
		const std::string& GetModelPath() const;

		bool IsNoCull() const { return no_cull_; }

		// Animation
		void SetAnimation(int index);
		void SetAnimation(const std::string& name);
		void UpdateAnimation(float dt);

		Animator* GetAnimator() const { return m_animator.get(); }
		std::shared_ptr<ModelData> GetData() const { return m_data; }

		// Manual bone manipulation
		void                     AddBone(const std::string& name, const std::string& parentName, const glm::mat4& localTransform);
		std::vector<std::string> GetEffectors() const;

		void           SetBoneConstraint(const std::string& boneName, const BoneConstraint& constraint);
		BoneConstraint GetBoneConstraint(const std::string& boneName) const;

		glm::vec3 GetBoneWorldPosition(const std::string& boneName) const;
		void      SetBoneWorldPosition(const std::string& boneName, const glm::vec3& worldPos);
		glm::quat GetBoneWorldRotation(const std::string& boneName) const;
		void      SetBoneWorldRotation(const std::string& boneName, const glm::quat& worldRot);

		void SkinToHierarchy();

		// IK
		void SolveIK(
			const std::string&      effectorName,
			const glm::vec3&        targetWorldPos,
			float                   tolerance = 0.01f,
			int                     maxIterations = 20,
			const std::string&      rootBoneName = "",
			const std::vector<std::string>& lockedBones = {}
		);

		void SolveIK(
			const std::string&      effectorName,
			const glm::vec3&        targetWorldPos,
			const glm::quat&        targetWorldRot,
			float                   tolerance = 0.01f,
			int                     maxIterations = 20,
			const std::string&      rootBoneName = "",
			const std::vector<std::string>& lockedBones = {}
		);

		// Exposed for AssetManager to fill during loading
		friend class AssetManager;

	private:
		void EnsureUniqueModelData();

		// Model data
		glm::quat                  base_rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		std::shared_ptr<ModelData> m_data;
		std::unique_ptr<Animator>  m_animator;
		bool                       no_cull_ = false;

		float dissolve_sweep_ = 0.0f;
		bool  use_dissolve_sweep_ = false;

		static unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma = false);
	};

} // namespace Boidsish
