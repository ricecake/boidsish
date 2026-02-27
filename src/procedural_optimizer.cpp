#include "procedural_optimizer.h"
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <map>
#include <set>

namespace Boidsish {

    void ProceduralOptimizer::Optimize(ProceduralIR& ir) {
        ConsolidateTubes(ir);
        HandleJunctions(ir);
    }

    void ProceduralOptimizer::ConsolidateTubes(ProceduralIR& ir) {
        if (ir.elements.empty()) return;

        bool changed = true;
        while (changed) {
            changed = false;
            std::vector<bool> to_remove(ir.elements.size(), false);

            for (int i = 0; i < (int)ir.elements.size(); ++i) {
                if (to_remove[i]) continue;
                auto& e1 = ir.elements[i];
                if (e1.type != ProceduralElementType::Tube) continue;
                if (e1.children.size() != 1) continue;

                int child_idx = e1.children[0];
                auto& e2 = ir.elements[child_idx];
                if (e2.type != ProceduralElementType::Tube) continue;
                if (to_remove[child_idx]) continue;

                // Check if they are collinear and have similar color
                glm::vec3 d1 = glm::normalize(e1.end_position - e1.position);
                glm::vec3 d2 = glm::normalize(e2.end_position - e2.position);

                if (glm::dot(d1, d2) > 0.999f && glm::distance2(e1.color, e2.color) < 0.001f) {
                    // Merge e2 into e1
                    e1.end_position = e2.end_position;
                    e1.end_radius = e2.end_radius;
                    e1.length = glm::distance(e1.position, e1.end_position);
                    e1.children = e2.children;

                    // Update children's parent pointer
                    for (int child_of_child : e2.children) {
                        ir.elements[child_of_child].parent = i;
                    }

                    to_remove[child_idx] = true;
                    changed = true;
                }
            }

            if (changed) {
                std::vector<ProceduralElement> new_elements;
                std::map<int, int> old_to_new;
                for (int i = 0; i < (int)ir.elements.size(); ++i) {
                    if (!to_remove[i]) {
                        old_to_new[i] = (int)new_elements.size();
                        new_elements.push_back(ir.elements[i]);
                    }
                }

                for (auto& e : new_elements) {
                    if (e.parent != -1) e.parent = old_to_new[e.parent];
                    for (int& child : e.children) {
                        child = old_to_new[child];
                    }
                }
                ir.elements = std::move(new_elements);
            }
        }
    }

    void ProceduralOptimizer::HandleJunctions(ProceduralIR& ir) {
        // Identify points where multiple tubes meet and insert hubs
        std::vector<ProceduralElement> hubs_to_add;

        for (int i = 0; i < (int)ir.elements.size(); ++i) {
            auto& e = ir.elements[i];
            if (e.type == ProceduralElementType::Tube) {
                if (e.children.size() > 1) {
                    // Junction at end_position
                    ProceduralElement hub;
                    hub.type = ProceduralElementType::Hub;
                    hub.position = e.end_position;
                    hub.radius = e.end_radius * 1.1f; // Slightly larger for better blending
                    hub.color = e.color;
                    hub.parent = i;
                    hub.children = e.children;

                    int hub_idx = (int)ir.elements.size() + (int)hubs_to_add.size();

                    // Re-route children to the hub
                    for (int child_idx : e.children) {
                        ir.elements[child_idx].parent = hub_idx;
                    }
                    e.children = { hub_idx };

                    hubs_to_add.push_back(hub);
                }
            }
        }

        for (const auto& hub : hubs_to_add) {
            ir.elements.push_back(hub);
        }
    }

} // namespace Boidsish
