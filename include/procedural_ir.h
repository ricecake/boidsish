#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

    enum class ProceduralElementType {
        Tube,
        Hub,
        Leaf,
        Puffball
    };

    struct ProceduralElement {
        ProceduralElementType type;

        // Geometric properties
        glm::vec3 position;      // For Hub, Leaf, Puffball: center. For Tube: start point.
        glm::vec3 end_position;  // For Tube: end point.
        glm::quat orientation;   // For Leaf: orientation.

        float radius;            // For Hub, Puffball: radius. For Tube: start radius.
        float end_radius;        // For Tube: end radius.
        float length;            // Calculated length for Tubes.

        glm::vec3 color;

        // Hierarchy
        int parent = -1;
        std::vector<int> children;

        // Metadata
        float intensity = 1.0f; // Could be used for SDF influence
    };

    struct ProceduralIR {
        std::vector<ProceduralElement> elements;
        std::string name;

        void AddElement(const ProceduralElement& element) {
            elements.push_back(element);
        }

        int AddTube(glm::vec3 start, glm::vec3 end, float start_r, float end_r, glm::vec3 col, int parent_idx = -1) {
            ProceduralElement e;
            e.type = ProceduralElementType::Tube;
            e.position = start;
            e.end_position = end;
            e.radius = start_r;
            e.end_radius = end_r;
            e.length = glm::distance(start, end);
            e.color = col;
            e.parent = parent_idx;

            int idx = static_cast<int>(elements.size());
            elements.push_back(e);

            if (parent_idx != -1 && parent_idx < idx) {
                elements[parent_idx].children.push_back(idx);
            }
            return idx;
        }

        int AddHub(glm::vec3 pos, float r, glm::vec3 col, int parent_idx = -1) {
            ProceduralElement e;
            e.type = ProceduralElementType::Hub;
            e.position = pos;
            e.radius = r;
            e.color = col;
            e.parent = parent_idx;

            int idx = static_cast<int>(elements.size());
            elements.push_back(e);

            if (parent_idx != -1 && parent_idx < idx) {
                elements[parent_idx].children.push_back(idx);
            }
            return idx;
        }

        int AddLeaf(glm::vec3 pos, glm::quat ori, float size, glm::vec3 col, int parent_idx = -1) {
            ProceduralElement e;
            e.type = ProceduralElementType::Leaf;
            e.position = pos;
            e.orientation = ori;
            e.radius = size; // Using radius as size
            e.color = col;
            e.parent = parent_idx;

            int idx = static_cast<int>(elements.size());
            elements.push_back(e);

            if (parent_idx != -1 && parent_idx < idx) {
                elements[parent_idx].children.push_back(idx);
            }
            return idx;
        }

        int AddPuffball(glm::vec3 pos, float r, glm::vec3 col, int parent_idx = -1) {
            ProceduralElement e;
            e.type = ProceduralElementType::Puffball;
            e.position = pos;
            e.radius = r;
            e.color = col;
            e.parent = parent_idx;

            int idx = static_cast<int>(elements.size());
            elements.push_back(e);

            if (parent_idx != -1 && parent_idx < idx) {
                elements[parent_idx].children.push_back(idx);
            }
            return idx;
        }
    };

} // namespace Boidsish
