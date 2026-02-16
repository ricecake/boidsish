#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "shape.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	/**
	 * @brief A 3D shape that computes Delaunay tetrahedralization from control points.
	 *
	 * Points are individually addressable via stable IDs, allowing entities (like boids)
	 * to drive specific control points. The mesh automatically re-tetrahedralizes as points
	 * move, rendering only the outer surface (boundary faces) to create a dynamic "blob".
	 *
	 * Uses 3D Bowyer-Watson algorithm for incremental Delaunay tetrahedralization.
	 * The rendered surface consists of triangular faces that appear on exactly one
	 * tetrahedron (the convex hull boundary).
	 */
	class DelaunayBlob: public Shape {
	public:
		/// Control point with stable ID for entity binding
		struct ControlPoint {
			int       id;
			glm::vec3 position;
			glm::vec3 velocity{0.0f}; // Optional: for smoothing/interpolation
			glm::vec4 color{1.0f};    // Per-point color (blends across surface)
		};

		/// Tetrahedron from 3D Delaunay tetrahedralization
		struct Tetrahedron {
			std::array<int, 4> vertices; // Point IDs (not indices)
			glm::vec3          circumcenter;
			float              circumradius_sq;
		};

		/// Triangular face (for surface rendering)
		struct Face {
			std::array<int, 3> vertices; // Point IDs, ordered for consistent normal
			glm::vec3          normal;
			glm::vec3          centroid;

			bool operator==(const Face& other) const;
			bool operator<(const Face& other) const;
		};

		enum class RenderMode {
			Solid,         // Render surface as solid mesh
			Wireframe,     // Render surface edges only
			SolidWithWire, // Solid fill with wireframe overlay
			Transparent    // Render with alpha blending
		};

	public:
		DelaunayBlob();
		~DelaunayBlob() override;

		// Disable copy (OpenGL resources)
		DelaunayBlob(const DelaunayBlob&) = delete;
		DelaunayBlob& operator=(const DelaunayBlob&) = delete;

		// Move semantics
		DelaunayBlob(DelaunayBlob&& other) noexcept;
		DelaunayBlob& operator=(DelaunayBlob&& other) noexcept;

		// === Point Management ===

		/// Add a new control point, returns its stable ID
		int AddPoint(const glm::vec3& position);

		/// Add a point with a specific ID (useful for entity binding)
		/// Returns false if ID already exists
		bool AddPointWithId(int point_id, const glm::vec3& position);

		/// Remove a point by ID
		void RemovePoint(int point_id);

		/// Update a point's position by ID
		void SetPointPosition(int point_id, const glm::vec3& position);

		/// Update a point's position and velocity
		void SetPointState(int point_id, const glm::vec3& position, const glm::vec3& velocity);

		/// Set per-point color
		void SetPointColor(int point_id, const glm::vec4& color);

		/// Get a point's current position (returns nullopt if not found)
		std::optional<glm::vec3> GetPointPosition(int point_id) const;

		/// Get all point IDs
		std::vector<int> GetPointIds() const;

		/// Get point count
		size_t GetPointCount() const { return points_.size(); }

		/// Check if a point exists
		bool HasPoint(int point_id) const { return points_.count(point_id) > 0; }

		// === Bulk Operations ===

		/// Add multiple points at once (more efficient), returns vector of IDs
		std::vector<int> AddPoints(const std::vector<glm::vec3>& positions);

		/// Update multiple points at once
		void SetPointPositions(const std::map<int, glm::vec3>& positions);

		/// Clear all points
		void Clear();

		// === Tetrahedralization ===

		/// Recompute Delaunay tetrahedralization (call after point updates, or use auto-update)
		void Retetrahedralize();

		/// Enable/disable automatic re-tetrahedralization on point changes
		void SetAutoRetetrahedralize(bool enable) { auto_retetrahedralize_ = enable; }

		/// Get the computed tetrahedra (read-only)
		const std::vector<Tetrahedron>& GetTetrahedra() const { return tetrahedra_; }

		/// Get boundary surface faces
		const std::vector<Face>& GetSurfaceFaces() const { return surface_faces_; }

		// === Rendering Configuration ===

		void SetRenderMode(RenderMode mode) {
			render_mode_ = mode;
			MarkDirty();
		}

		RenderMode GetRenderMode() const { return render_mode_; }

		/// Set wireframe color (used in Wireframe and SolidWithWire modes)
		void SetWireframeColor(const glm::vec4& color) { wireframe_color_ = color; }

		/// Enable smooth normals (averaged at vertices) vs flat shading
		void SetSmoothNormals(bool smooth) {
			smooth_normals_ = smooth;
			MarkDirty();
		}

		/// Set alpha for transparency
		void SetAlpha(float alpha) { alpha_ = alpha; }

		// === Shape Interface ===

		using Shape::render;
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		std::string GetInstanceKey() const override { return "delaunay_blob_" + std::to_string(GetId()); }

		/// Get centroid of all points
		glm::vec3 GetCentroid() const;

		/// Get bounding radius from centroid
		float GetBoundingRadius() const;

	protected:
		void MarkDirty() const { mesh_dirty_ = true; }

		void UpdateMeshBuffers() const;
		void InitializeBuffers() const;
		void CleanupBuffers();

	private:
		// === 3D Delaunay Algorithm (Bowyer-Watson) ===

		/// Compute 3D Delaunay tetrahedralization
		void ComputeDelaunay3D() const;

		/// Extract boundary surface faces from tetrahedralization
		void ExtractSurfaceFaces() const;

		/// Check if point is inside tetrahedron's circumsphere
		bool InCircumsphere(const glm::vec3& p, const Tetrahedron& tet) const;

		/// Compute circumsphere of tetrahedron (center and radius²)
		std::pair<glm::vec3, float>
		ComputeCircumsphere(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) const;

		/// Create super-tetrahedron containing all points
		std::array<glm::vec3, 4> ComputeSuperTetrahedron() const;

		/// Compute face normal (outward-facing from tetrahedron)
		glm::vec3 ComputeFaceNormal(
			const glm::vec3& p0,
			const glm::vec3& p1,
			const glm::vec3& p2,
			const glm::vec3& opposite
		) const;

		/// Create a canonical face representation (sorted vertex IDs)
		Face MakeFace(int v0, int v1, int v2) const;

	private:
		// Point storage with stable IDs
		std::map<int, ControlPoint> points_;
		int                         next_point_id_ = 0;

		// Rendering state
		RenderMode   render_mode_ = RenderMode::Solid;
		float        alpha_ = 1.0f;
		glm::vec4    wireframe_color_{0.1f, 0.1f, 0.1f, 1.0f};
		bool         smooth_normals_ = true;
		bool         auto_retetrahedralize_ = true;
		mutable bool mesh_dirty_ = true;

		// OpenGL resources (mutable for lazy initialization in const render)
		mutable GLuint vao_ = 0;
		mutable GLuint vbo_ = 0;
		mutable GLuint ebo_ = 0;
		mutable GLuint wire_ebo_ = 0;
		mutable size_t index_count_ = 0;
		mutable size_t wire_index_count_ = 0;
		mutable bool   buffers_initialized_ = false;

		// Cached tetrahedralization (mutable for lazy computation)
		mutable std::vector<Tetrahedron> tetrahedra_;
		mutable std::vector<Face>        surface_faces_;

		// Vertex data structure for GPU
		struct Vertex {
			glm::vec3 position;
			glm::vec3 normal;
			glm::vec4 color;
		};
	};

} // namespace Boidsish
