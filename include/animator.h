#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "model.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class Animator {
	public:
		Animator() = default;
		Animator(std::shared_ptr<ModelData> modelData);

		void UpdateAnimation(float dt);
		void PlayAnimation(int animationIndex);
		void PlayAnimation(const std::string& name);

		const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

		void SetModelData(std::shared_ptr<ModelData> modelData);

		float GetCurrentTime() const { return m_CurrentTime; }

		int GetCurrentAnimationIndex() const { return m_CurrentAnimationIndex; }

		// Manual bone control
		void      SetBoneLocalTransform(const std::string& boneName, const glm::mat4& transform);
		glm::mat4 GetBoneLocalTransform(const std::string& boneName) const;
		glm::mat4 GetBoneModelSpaceTransform(const std::string& boneName) const;
		std::string GetBoneParentName(const std::string& boneName) const;

	private:
		void CalculateBoneTransform(const NodeData& node, glm::mat4 parentTransform);

		std::vector<glm::mat4>     m_FinalBoneMatrices;
		std::shared_ptr<ModelData> m_ModelData;
		float                      m_CurrentTime = 0.0f;
		int                        m_CurrentAnimationIndex = -1;

		std::map<std::string, glm::mat4> m_LocalOverrides;
		std::map<std::string, glm::mat4> m_GlobalMatrices; // Model-space transforms of nodes
	};

} // namespace Boidsish
