#pragma once

#include <deque>
#include <tuple>
#include <vector>

#include "shader.h"
#include "vector.h"
#include "constants.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	class Trail {
	public:
		Trail(int max_length = Constants::Class::Trails::DefaultMaxLength());
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

		// Get vertex data for batched rendering (interleaved pos + normal + color)
		std::vector<float> GetInterleavedVertexData() const;
		size_t             GetMaxVertexCount() const { return mesh_vertices.size(); }
		size_t             GetHead() const { return head; }
		size_t             GetTail() const { return tail; }
		size_t             GetVertexCount() const { return vertex_count; }
		bool               IsFull() const { return full; }
		bool               GetIridescent() const { return iridescent_; }
		bool               GetUseRocketTrail() const { return useRocketTrail_; }
		float              GetBaseThickness() const { return BASE_THICKNESS; }
		bool               IsDirty() const { return mesh_dirty; }
		void               ClearDirty() { mesh_dirty = false; }

	private:
		struct TrailVertex {
			glm::vec3 pos;
			glm::vec3 normal;
			glm::vec3 color;
		};

		// Catmull-Rom interpolation for smooth curves
		Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) const;

		// Appends a new segment to the geometry cache
		void AppendToGeometryCache(
			const Vector3& p0,
			const Vector3& p1,
			const Vector3& p2,
			const Vector3& p3,
			const Vector3& c0,
			const Vector3& c1,
			const Vector3& c2,
			const Vector3& c3
		);
		// Removes the oldest segment from the geometry cache
		void PopFromGeometryCache();
		void UpdateAndAppendSegment();

		// Frame transport for maintaining smooth normal orientation
		Vector3
		TransportFrame(const Vector3& prev_normal, const Vector3& prev_tangent, const Vector3& curr_tangent) const;

		std::deque<std::pair<glm::vec3, glm::vec3>> points;
		int                                         max_length;
		GLuint                                      vao;
		GLuint                                      vbo;
		GLuint                                      ebo;

		// Mesh data
		mutable std::vector<TrailVertex>  mesh_vertices;
		mutable std::vector<unsigned int> indices;
		mutable int                       vertex_count;
		mutable bool                      mesh_dirty;
		mutable size_t                    head = 0;
		mutable size_t                    tail = 0;
		mutable size_t                    old_tail = 0;
		mutable bool                      full = false;

		// Cached geometry data for incremental updates
		mutable std::deque<Vector3>                curve_positions;
		mutable std::deque<Vector3>                curve_colors;
		mutable std::deque<Vector3>                tangents;
		mutable std::deque<Vector3>                normals;
		mutable std::deque<Vector3>                binormals;
		mutable std::deque<std::vector<glm::vec3>> ring_positions;
		mutable std::deque<std::vector<glm::vec3>> ring_normals;
		bool                                       iridescent_ = false;
		bool                                       useRocketTrail_ = false;
		bool                                       usePBR_ = false;
		float                                      roughness_ = Constants::Class::Trails::DefaultRoughness();
		float                                      metallic_ = Constants::Class::Trails::DefaultMetallic();
		bool                                       managed_by_render_manager_ = false;

		// Configuration
		const int   TRAIL_SEGMENTS = Constants::Class::Trails::Segments();                        // Circular segments around trail
		const int   CURVE_SEGMENTS = Constants::Class::Trails::CurveSegments();                        // Interpolation segments per point
		const int   VERTS_PER_STEP = (TRAIL_SEGMENTS + 1) * 2; // For 8 segments, this is 18
		const float BASE_THICKNESS = Constants::Class::Trails::BaseThickness();                    // Maximum thickness at trail start
	};

} // namespace Boidsish
