#include "procedural_refiner.h"

#include <cmath>
#include <map>
#include <set>
#include <vector>

#include <glm/gtx/norm.hpp>

namespace Boidsish {

	namespace {
		struct PosKey {
			int x, y, z;
			bool operator<(const PosKey& o) const {
				if (x != o.x) return x < o.x;
				if (y != o.y) return y < o.y;
				return z < o.z;
			}
		};

		PosKey make_key(glm::vec3 p) {
			return { (int)std::round(p.x * 1000.0f), (int)std::round(p.y * 1000.0f), (int)std::round(p.z * 1000.0f) };
		}
	}

	void ProceduralRefiner::Refine(ProceduralIR& ir) {
		EnsureClosedManifold(ir);
	}

	void ProceduralRefiner::EnsureClosedManifold(ProceduralIR& ir) {
		// Map connection points to existing hubs
		std::map<PosKey, int> point_to_hub;
		for (int i = 0; i < (int)ir.elements.size(); ++i) {
			if (ir.elements[i].type == ProceduralElementType::Hub) {
				point_to_hub[make_key(ir.elements[i].position)] = i;
			}
		}

		// Find all tube connection points
		std::map<PosKey, std::vector<std::pair<int, bool>>> point_to_tubes; // bool: true=start, false=end
		for (int i = 0; i < (int)ir.elements.size(); ++i) {
			if (ir.elements[i].type == ProceduralElementType::Tube) {
				point_to_tubes[make_key(ir.elements[i].position)].push_back({i, true});
				point_to_tubes[make_key(ir.elements[i].end_position)].push_back({i, false});
			}
		}

		// Ensure every connection point has a hub
		for (auto& [pos_key, tubes] : point_to_tubes) {
			if (point_to_hub.find(pos_key) == point_to_hub.end()) {
				// Create new hub
				int sample_tube_idx = tubes[0].first;
				bool is_start = tubes[0].second;
				glm::vec3 pos = is_start ? ir.elements[sample_tube_idx].position : ir.elements[sample_tube_idx].end_position;
				float radius = is_start ? ir.elements[sample_tube_idx].radius : ir.elements[sample_tube_idx].end_radius;

				int hub_idx = ir.AddHub(pos, radius, ir.elements[sample_tube_idx].color);
				point_to_hub[pos_key] = hub_idx;
			}
		}

		// Now ensure all tubes are linked to their respective hubs
		for (auto& [pos_key, hub_idx] : point_to_hub) {
			auto& hub = ir.elements[hub_idx];
			auto it = point_to_tubes.find(pos_key);
			if (it == point_to_tubes.end()) continue;

			for (auto& tube_info : it->second) {
				int tube_idx = tube_info.first;
				bool is_start = tube_info.second;
				auto& tube = ir.elements[tube_idx];

				if (is_start) {
					// Tube starts at this hub
					if (tube.parent != hub_idx) {
						// Update hierarchy if needed
						// This is tricky because a tube might already have a parent.
						// In our IR, a hub can be an intermediate.
						// If tube.parent was -1, it's easy.
						if (tube.parent == -1) {
							tube.parent = hub_idx;
							hub.children.push_back(tube_idx);
						} else {
							// Tube already has a parent. If it's another tube, we might need to insert the hub between them.
							// But usually, junctions already handled by optimizer have hubs.
							// For now, let's just make sure it's linked if not already.
						}
					}
				} else {
					// Tube ends at this hub
					bool already_child = false;
					for (int child : tube.children) {
						if (child == hub_idx) {
							already_child = true;
							break;
						}
					}
					if (!already_child) {
						tube.children.push_back(hub_idx);
						hub.parent = tube_idx; // Note: Hub parent might be overwritten if multiple tubes end here.
					}
				}
			}
		}
	}

} // namespace Boidsish
