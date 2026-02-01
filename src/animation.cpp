#include "animation.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "logger.h"

namespace Boidsish {

    Animation::Animation(const std::string& animationPath, Model* model) {
        std::string normalized_path = animationPath;
        std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');

        Assimp::Importer importer;
        const aiScene*   scene = importer.ReadFile(normalized_path, aiProcess_Triangulate);

        if (!scene || !scene->mRootNode) {
            logger::ERROR("Failed to load animation at path: " + normalized_path);
            return;
        }

        if (scene->mNumAnimations == 0) {
            logger::ERROR("No animations found in file: " + normalized_path);
            return;
        }

        auto animation = scene->mAnimations[0];
        m_Duration = (float)animation->mDuration;
        m_TicksPerSecond = (int)animation->mTicksPerSecond;
        ReadHierarchyData(m_RootNode, scene->mRootNode);
        ReadMissingBones(animation, *model);
        m_IsValid = true;
    }

    Bone* Animation::FindBone(const std::string& name) {
        auto iter = std::find_if(m_Bones.begin(), m_Bones.end(),
            [&](const Bone& bone) {
                return bone.GetBoneName() == name;
            }
        );
        if (iter == m_Bones.end()) return nullptr;
        else return &(*iter);
    }

    void Animation::ReadMissingBones(const aiAnimation* animation, Model& model) {
        int size = animation->mNumChannels;

        auto& boneInfoMap = model.GetBoneInfoMap();
        int& boneCount = model.GetBoneCount();

        for (int i = 0; i < size; i++) {
            auto channel = animation->mChannels[i];
            std::string boneName = channel->mNodeName.data;

            if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
                boneInfoMap[boneName].id = boneCount;
                boneCount++;
            }
            m_Bones.push_back(Bone(channel->mNodeName.data,
                boneInfoMap[channel->mNodeName.data].id, channel));
        }

        m_BoneInfoMap = boneInfoMap;
    }

    void Animation::ReadHierarchyData(AssimpNodeData& dest, const aiNode* src) {
        assert(src);

        dest.name = src->mName.data;
        dest.transformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(src->mTransformation);
        dest.childrenCount = src->mNumChildren;

        for (int i = 0; i < src->mNumChildren; i++) {
            AssimpNodeData newData;
            ReadHierarchyData(newData, src->mChildren[i]);
            dest.children.push_back(newData);
        }
    }

}
