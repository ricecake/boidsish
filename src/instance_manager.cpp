#include "instance_manager.h"
#include <GL/glew.h>
#include "model.h"

namespace Boidsish {

void InstanceManager::AddInstance(std::shared_ptr<Shape> shape) {
    m_instance_groups[std::type_index(typeid(*shape))].shapes.push_back(shape);
}

void InstanceManager::Render(Shader& shader) {
    shader.use();
    shader.setBool("is_instanced", true);

    for (auto& pair : m_instance_groups) {
        auto& group = pair.second;
        if (group.shapes.empty()) {
            continue;
        }

        group.instance_data_.clear();
        group.instance_data_.reserve(group.shapes.size());
        for (const auto& shape : group.shapes) {
            group.instance_data_.push_back({shape->GetModelMatrix(), glm::vec4(shape->GetR(), shape->GetG(), shape->GetB(), shape->GetA())});
        }

        if (group.instance_vbo_ == 0) {
            glGenBuffers(1, &group.instance_vbo_);
        }

        glBindBuffer(GL_ARRAY_BUFFER, group.instance_vbo_);
        if (group.instance_data_.size() > group.capacity_) {
            glBufferData(GL_ARRAY_BUFFER, group.instance_data_.size() * sizeof(InstanceData), &group.instance_data_[0], GL_DYNAMIC_DRAW);
            group.capacity_ = group.instance_data_.size();
        } else {
            glBufferSubData(GL_ARRAY_BUFFER, 0, group.instance_data_.size() * sizeof(InstanceData), &group.instance_data_[0]);
        }

        group.shapes[0]->render_instanced(shader, group.shapes.size());
    }
    m_instance_groups.clear();
    shader.setBool("is_instanced", false);
}

}
