#pragma once

#include <map>
#include <memory>
#include <vector>
#include <typeindex>

#include "shape.h"
#include <shader.h>

namespace Boidsish {

struct InstanceData {
    glm::mat4 model_matrix;
    glm::vec4 color;
};

class InstanceManager {
public:
    void AddInstance(std::shared_ptr<Shape> shape);
    void Render(Shader& shader);

private:
    struct InstanceGroup {
        std::vector<std::shared_ptr<Shape>> shapes;
        std::vector<InstanceData> instance_data_;
        unsigned int instance_vbo_ = 0;
        size_t capacity_ = 0;
    };

    std::map<std::type_index, InstanceGroup> m_instance_groups;
};

}
