#pragma once

#include <deque>
#include <tuple>
#include <vector>

#include "constants.h"
#include "shader.h"
#include "vector.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	class Trail {
	public:
		Trail(
			int   max_length = Constants::Class::Trails::DefaultMaxLength(),
			float thickness = Constants::Class::Trails::BaseThickness()
		);
		~Trail();

		void AddPoint(glm::vec3 position, glm::vec3 color);
		void Render(Shader& shader) const;
		void SetIridescence(bool enabled);
		void SetUseRocketTrail(bool enabled);

		void SetUsePBR(bool enabled) { usePBR_ = enabled; }

		void SetRoughness(float roughness) { roughness_ = roughness; }

		void SetMetallic(float metallic) { metallic_ = metallic; }

		bool GetUsePBR() const { return usePBR_; }

		float GetRoughness() const { return roughness_; }

		float GetMetallic() const { return metallic_; }

		// Batched rendering support
		void SetManagedByRenderManager(bool managed) { managed_by_render_manager_ = managed; }

		bool IsManagedByRenderManager() const { return managed_by_render_manager_; }

		// Get control point data
		const std::deque<std::pair<glm::vec3, glm::vec3>>& GetPoints() const { return points; }

		size_t GetHead() const { return head; }

		size_t GetTail() const { return tail; }

		size_t GetMaxPoints() const { return max_length; }

		bool IsFull() const { return full; }

		bool GetIridescent() const { return iridescent_; }

		bool GetUseRocketTrail() const { return useRocketTrail_; }

		float GetBaseThickness() const { return BASE_THICKNESS; }

		bool IsDirty() const { return mesh_dirty; }

		void ClearDirty() { mesh_dirty = false; }

		// Bounding box for culling
		glm::vec3 GetMinBound() const;
		glm::vec3 GetMaxBound() const;

	private:
		std::deque<std::pair<glm::vec3, glm::vec3>> points;
		int                                         max_length;
		float                                       thickness;

		// Metadata for GPU-side generation
		mutable bool   mesh_dirty = false;
		mutable size_t head = 0;
		mutable size_t tail = 0;
		mutable bool   full = false;

		bool  iridescent_ = false;
		bool  useRocketTrail_ = false;
		bool  usePBR_ = false;
		float roughness_ = Constants::Class::Trails::DefaultRoughness();
		float metallic_ = Constants::Class::Trails::DefaultMetallic();
		bool  managed_by_render_manager_ = false;

		// Bounding box cache
		mutable glm::vec3 min_bound_ = glm::vec3(0.0f);
		mutable glm::vec3 max_bound_ = glm::vec3(0.0f);
		mutable bool      bounds_dirty_ = true;

		void UpdateBounds() const;

		// Configuration
		const int   TRAIL_SEGMENTS = Constants::Class::Trails::Segments();      // Circular segments around trail
		const int   CURVE_SEGMENTS = Constants::Class::Trails::CurveSegments(); // Interpolation segments per point
		const int   VERTS_PER_STEP = (TRAIL_SEGMENTS + 1) * 2;                  // For 8 segments, this is 18
		const float BASE_THICKNESS = Constants::Class::Trails::BaseThickness(); // Maximum thickness at trail start
	};

} // namespace Boidsish
