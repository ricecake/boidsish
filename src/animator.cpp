#include "animator.h"
#include <glm/gtc/matrix_transform.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace Boidsish {

	Animator::Animator(std::shared_ptr<ModelData> modelData) {
		SetModelData(modelData);
	}

	void Animator::SetModelData(std::shared_ptr<ModelData> modelData) {
		m_ModelData = modelData;
		m_FinalBoneMatrices.clear();
		m_FinalBoneMatrices.resize(100, glm::mat4(1.0f));
	}

	void Animator::UpdateAnimation(float dt) {
		if (m_ModelData && m_CurrentAnimationIndex >= 0 && (size_t)m_CurrentAnimationIndex < m_ModelData->animations.size()) {
			auto& animation = m_ModelData->animations[m_CurrentAnimationIndex];
			float ticksPerSecond = (animation.ticksPerSecond != 0) ? (float)animation.ticksPerSecond : 24.0f;
			m_CurrentTime += ticksPerSecond * dt;
			m_CurrentTime = std::fmod(m_CurrentTime, animation.duration);
			CalculateBoneTransform(m_ModelData->root_node, glm::mat4(1.0f));
		} else if (m_ModelData) {
			// Even if no animation is playing, we should still update bone matrices to bind pose
			CalculateBoneTransform(m_ModelData->root_node, glm::mat4(1.0f));
		}
	}

	void Animator::PlayAnimation(int animationIndex) {
		m_CurrentAnimationIndex = animationIndex;
		m_CurrentTime = 0.0f;
	}

	void Animator::PlayAnimation(const std::string& name) {
		if (!m_ModelData) return;
		for (size_t i = 0; i < m_ModelData->animations.size(); i++) {
			if (m_ModelData->animations[i].name == name) {
				PlayAnimation((int)i);
				return;
			}
		}
	}

	void Animator::CalculateBoneTransform(const NodeData& node, glm::mat4 parentTransform) {
		std::string nodeName = node.name;
		glm::mat4 nodeTransform = node.transformation;

		// Check if this node has an animation
		if (m_CurrentAnimationIndex >= 0 && (size_t)m_CurrentAnimationIndex < m_ModelData->animations.size()) {
			auto& animation = m_ModelData->animations[m_CurrentAnimationIndex];
			// Find BoneAnimation for this node
			for (const auto& boneAnim : animation.boneAnimations) {
				if (boneAnim.name == nodeName) {
					// Interpolate Position
					glm::vec3 translation(0.0f);
					if (boneAnim.numPositions > 0) {
						if (boneAnim.numPositions == 1) {
							translation = boneAnim.positions[0].position;
						} else {
							int p0Index = 0;
							for (int i = 0; i < boneAnim.numPositions - 1; i++) {
								if (m_CurrentTime < boneAnim.positions[i + 1].timeStamp) {
									p0Index = i;
									break;
								}
							}
							int p1Index = p0Index + 1;
							float t0 = boneAnim.positions[p0Index].timeStamp;
							float t1 = boneAnim.positions[p1Index].timeStamp;
							float factor = (m_CurrentTime - t0) / (t1 - t0);
							translation = glm::mix(boneAnim.positions[p0Index].position, boneAnim.positions[p1Index].position, factor);
						}
					}

					// Interpolate Rotation
					glm::quat rotation(1, 0, 0, 0);
					if (boneAnim.numRotations > 0) {
						if (boneAnim.numRotations == 1) {
							rotation = boneAnim.rotations[0].orientation;
						} else {
							int r0Index = 0;
							for (int i = 0; i < boneAnim.numRotations - 1; i++) {
								if (m_CurrentTime < boneAnim.rotations[i + 1].timeStamp) {
									r0Index = i;
									break;
								}
							}
							int r1Index = r0Index + 1;
							float t0 = boneAnim.rotations[r0Index].timeStamp;
							float t1 = boneAnim.rotations[r1Index].timeStamp;
							float factor = (m_CurrentTime - t0) / (t1 - t0);
							rotation = glm::slerp(boneAnim.rotations[r0Index].orientation, boneAnim.rotations[r1Index].orientation, factor);
						}
					}

					// Interpolate Scale
					glm::vec3 scale(1.0f);
					if (boneAnim.numScalings > 0) {
						if (boneAnim.numScalings == 1) {
							scale = boneAnim.scales[0].scale;
						} else {
							int s0Index = 0;
							for (int i = 0; i < boneAnim.numScalings - 1; i++) {
								if (m_CurrentTime < boneAnim.scales[i + 1].timeStamp) {
									s0Index = i;
									break;
								}
							}
							int s1Index = s0Index + 1;
							float t0 = boneAnim.scales[s0Index].timeStamp;
							float t1 = boneAnim.scales[s1Index].timeStamp;
							float factor = (m_CurrentTime - t0) / (t1 - t0);
							scale = glm::mix(boneAnim.scales[s0Index].scale, boneAnim.scales[s1Index].scale, factor);
						}
					}

					nodeTransform = glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
					break;
				}
			}
		}

		glm::mat4 globalTransformation = parentTransform * nodeTransform;

		auto it = m_ModelData->bone_info_map.find(nodeName);
		if (it != m_ModelData->bone_info_map.end()) {
			int index = it->second.id;
			glm::mat4 offset = it->second.offset;
			if (index >= 0 && (size_t)index < m_FinalBoneMatrices.size()) {
				m_FinalBoneMatrices[index] = globalTransformation * offset;
			}
		} else {
			// Robust name matching for Assimp/FBX.
			// FBX often adds prefixes/suffixes or namespaces (e.g., "ModelName:BoneName")
			for (auto const& [boneName, info] : m_ModelData->bone_info_map) {
				bool match = false;

				// 1. Substring matching (carefully)
				// Check if nodeName contains boneName or vice versa, typically separated by ':' or '_'
				size_t nodeColon = nodeName.find_last_of(':');
				std::string nodeSimple = (nodeColon == std::string::npos) ? nodeName : nodeName.substr(nodeColon + 1);

				size_t boneColon = boneName.find_last_of(':');
				std::string boneSimple = (boneColon == std::string::npos) ? boneName : boneName.substr(boneColon + 1);

				if (nodeSimple == boneSimple) {
					match = true;
				} else if (nodeName.find(boneName) != std::string::npos || boneName.find(nodeName) != std::string::npos) {
					// Fallback to substring only if reasonable length to avoid false positives
					if (boneName.length() > 3 && nodeName.length() > 3) {
						match = true;
					}
				}

				if (match) {
					int index = info.id;
					glm::mat4 offset = info.offset;
					if (index >= 0 && (size_t)index < m_FinalBoneMatrices.size()) {
						m_FinalBoneMatrices[index] = globalTransformation * offset;
					}
					break;
				}
			}
		}

		for (const auto& child : node.children) {
			CalculateBoneTransform(child, globalTransformation);
		}
	}

} // namespace Boidsish
