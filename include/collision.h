#pragma once

#include "spatial_entity_handler.h"
#include <iostream>
#include <set>

namespace Boidsish {

class CollisionHandler : public SpatialEntityHandler {
protected:
    void PostTimestep(float time, float delta_time) override {
        // First, update the R-tree with the new entity positions
        SpatialEntityHandler::PostTimestep(time, delta_time);

        // Keep track of pairs we've already checked to avoid duplicate collision events
        std::set<std::pair<int, int>> checked_pairs;

        for (const auto& pair : GetAllEntities()) {
            auto& entity1 = pair.second;
            float search_radius = entity1->GetSize(); // Broad search radius

            float min_b[3] = {entity1->GetXPos() - search_radius, entity1->GetYPos() - search_radius, entity1->GetZPos() - search_radius};
            float max_b[3] = {entity1->GetXPos() + search_radius, entity1->GetYPos() + search_radius, entity1->GetZPos() + search_radius};

            rtree_.Search(min_b, max_b, [&](int id2) {
                if (entity1->GetId() == id2) {
                    return true; // Don't check against self
                }

                // Avoid duplicate checks
                int id1 = entity1->GetId();
                if (id1 > id2) std::swap(id1, id2);
                if (checked_pairs.count({id1, id2})) {
                    return true;
                }
                checked_pairs.insert({id1, id2});

                auto entity2 = GetEntity(id2);
                if (entity2) {
                    // Narrow phase: sphere-to-sphere collision check
                    float distance = entity1->GetPosition().DistanceTo(entity2->GetPosition());
                    float combined_size = entity1->GetSize() + entity2->GetSize();

                    if (distance < combined_size) {
                        // Collision detected!
                        entity1->OnCollision(*entity2);
                        entity2->OnCollision(*entity1);
                    }
                }
                return true; // Continue searching
            });
        }
    }
};

} // namespace Boidsish
