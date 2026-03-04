#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	enum class ProceduralElementType {
		Tube,
		Hub,
		Leaf,
		Puffball,
		ControlPoint, // Spline waypoint: preserves curve shape, generates no geometry
		Box,
		Wedge,
		Pyramid
	};

	enum class SkinningMode { Auto, Rigid, Smooth, None };

	struct ProceduralElement {
		ProceduralElementType type;

		// Geometric properties
		glm::vec3 position = glm::vec3(0.0f);     // For Hub, Leaf, Puffball, etc: center/base. For Tube: start point.
		glm::vec3 end_position = glm::vec3(0.0f); // For Tube: end point.
		glm::quat orientation = glm::quat(1, 0, 0, 0); // For Leaf, Box, Wedge, Pyramid: orientation.

		float radius = 0.0f;     // For Hub, Puffball: radius. For Tube: start radius.
		float end_radius = 0.0f; // For Tube: end radius.
		float length = 0.0f;     // Calculated length for Tubes.
		glm::vec3 dimensions = glm::vec3(1.0f); // For Box, Wedge, Pyramid

		glm::vec3 color = glm::vec3(1.0f);
		int       variant = 0; // Shape variant for Leaf, etc.

		// Hierarchy
		int              parent = -1;
		std::vector<int> children;

		// Metadata
		float        intensity = 1.0f; // Could be used for SDF influence
		std::string  name;
		bool         is_bone = false;
		SkinningMode skinning_mode = SkinningMode::Auto;
	};

	struct ProceduralIR {
		std::vector<ProceduralElement> elements;
		std::string                    name;

		void AddElement(const ProceduralElement& element) { elements.push_back(element); }

		int AddTube(
			glm::vec3          start,
			glm::vec3          end,
			float              start_r,
			float              end_r,
			glm::vec3          col,
			int                parent_idx = -1,
			const std::string& name = "",
			bool               is_bone = false
		) {
			ProceduralElement e;
			e.type = ProceduralElementType::Tube;
			e.position = start;
			e.end_position = end;
			e.radius = start_r;
			e.end_radius = end_r;
			e.length = glm::distance(start, end);
			e.color = col;
			e.parent = parent_idx;
			e.name = name;
			e.is_bone = is_bone;

			int idx = static_cast<int>(elements.size());
			elements.push_back(e);

			if (parent_idx != -1 && parent_idx < idx) {
				elements[parent_idx].children.push_back(idx);
			}
			return idx;
		}

		int AddHub(
			glm::vec3          pos,
			float              r,
			glm::vec3          col,
			int                parent_idx = -1,
			const std::string& name = "",
			bool               is_bone = false
		) {
			ProceduralElement e;
			e.type = ProceduralElementType::Hub;
			e.position = pos;
			e.radius = r;
			e.color = col;
			e.parent = parent_idx;
			e.name = name;
			e.is_bone = is_bone;

			int idx = static_cast<int>(elements.size());
			elements.push_back(e);

			if (parent_idx != -1 && parent_idx < idx) {
				elements[parent_idx].children.push_back(idx);
			}
			return idx;
		}

		int AddLeaf(
			glm::vec3          pos,
			glm::quat          ori,
			float              size,
			glm::vec3          col,
			int                variant = 0,
			int                parent_idx = -1,
			const std::string& name = "",
			bool               is_bone = false
		) {
			ProceduralElement e;
			e.type = ProceduralElementType::Leaf;
			e.position = pos;
			e.orientation = ori;
			e.radius = size; // Using radius as size
			e.color = col;
			e.variant = variant;
			e.parent = parent_idx;
			e.name = name;
			e.is_bone = is_bone;

			int idx = static_cast<int>(elements.size());
			elements.push_back(e);

			if (parent_idx != -1 && parent_idx < idx) {
				elements[parent_idx].children.push_back(idx);
			}
			return idx;
		}

		int AddPuffball(
			glm::vec3          pos,
			float              r,
			glm::vec3          col,
			int                variant = 0,
			int                parent_idx = -1,
			const std::string& name = "",
			bool               is_bone = false
		) {
			ProceduralElement e;
			e.type = ProceduralElementType::Puffball;
			e.position = pos;
			e.radius = r;
			e.color = col;
			e.variant = variant;
			e.parent = parent_idx;
			e.name = name;
			e.is_bone = is_bone;

			int idx = static_cast<int>(elements.size());
			elements.push_back(e);

			if (parent_idx != -1 && parent_idx < idx) {
				elements[parent_idx].children.push_back(idx);
			}
			return idx;
		}

		int AddBox(
			glm::vec3          pos,
			glm::quat          ori,
			glm::vec3          dims,
			glm::vec3          col,
			int                parent_idx = -1,
			const std::string& name = "",
			bool               is_bone = false
		) {
			ProceduralElement e;
			e.type = ProceduralElementType::Box;
			e.position = pos;
			e.orientation = ori;
			e.dimensions = dims;
			e.color = col;
			e.parent = parent_idx;
			e.name = name;
			e.is_bone = is_bone;

			int idx = static_cast<int>(elements.size());
			elements.push_back(e);

			if (parent_idx != -1 && parent_idx < idx) {
				elements[parent_idx].children.push_back(idx);
			}
			return idx;
		}

		int AddWedge(
			glm::vec3          pos,
			glm::quat          ori,
			glm::vec3          dims,
			glm::vec3          col,
			int                parent_idx = -1,
			const std::string& name = "",
			bool               is_bone = false
		) {
			ProceduralElement e;
			e.type = ProceduralElementType::Wedge;
			e.position = pos;
			e.orientation = ori;
			e.dimensions = dims;
			e.color = col;
			e.parent = parent_idx;
			e.name = name;
			e.is_bone = is_bone;

			int idx = static_cast<int>(elements.size());
			elements.push_back(e);

			if (parent_idx != -1 && parent_idx < idx) {
				elements[parent_idx].children.push_back(idx);
			}
			return idx;
		}

		int AddPyramid(
			glm::vec3          pos,
			glm::quat          ori,
			glm::vec3          dims,
			glm::vec3          col,
			int                parent_idx = -1,
			const std::string& name = "",
			bool               is_bone = false
		) {
			ProceduralElement e;
			e.type = ProceduralElementType::Pyramid;
			e.position = pos;
			e.orientation = ori;
			e.dimensions = dims;
			e.color = col;
			e.parent = parent_idx;
			e.name = name;
			e.is_bone = is_bone;

			int idx = static_cast<int>(elements.size());
			elements.push_back(e);

			if (parent_idx != -1 && parent_idx < idx) {
				elements[parent_idx].children.push_back(idx);
			}
			return idx;
		}

		int AddControlPoint(glm::vec3 pos, float r, glm::vec3 col, int parent_idx = -1) {
			ProceduralElement e;
			e.type = ProceduralElementType::ControlPoint;
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
