#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "terrain_deformation_manager.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class Terrain;
	class TerrainRenderManager;
	struct Frustum;
	struct Camera;

	/**
	 * @brief Interface for terrain generation systems.
	 *
	 * This abstract base class defines the contract that all terrain generators
	 * must fulfill. It provides a standard interface for:
	 * - Chunk-based terrain streaming and visibility
	 * - Height and normal queries at arbitrary world positions
	 * - Raycasting against terrain
	 * - Terrain deformations (craters, flattening, etc.)
	 * - Integration with the TerrainRenderManager for GPU-accelerated rendering
	 *
	 * Implementations can provide different terrain generation algorithms
	 * (procedural noise, heightmap-based, voxel-based, etc.) while maintaining
	 * compatibility with the rest of the engine.
	 *
	 * Example custom implementation:
	 * @code
	 * class MyCustomTerrain : public ITerrainGenerator {
	 *     // Implement all pure virtual methods
	 *     // Can use different noise functions, data sources, etc.
	 * };
	 * @endcode
	 */
	class ITerrainGenerator {
	public:
		virtual ~ITerrainGenerator() = default;

		// ==================== Core Update and Visibility ====================

		/**
		 * @brief Update terrain streaming based on camera position.
		 *
		 * Called each frame to:
		 * - Load/generate new chunks that come into view
		 * - Unload chunks that move out of view distance
		 * - Process pending async chunk generation tasks
		 * - Register chunks with the render manager
		 *
		 * @param frustum Current view frustum for visibility culling
		 * @param camera Current camera state (position, direction, etc.)
		 */
		virtual void Update(const Frustum& frustum, const Camera& camera) = 0;

		/**
		 * @brief Get visible terrain chunks for rendering.
		 *
		 * Returns a reference to the internal visible chunks list.
		 * This list is updated by Update() and should only be accessed
		 * from the main thread.
		 *
		 * @return Const reference to visible chunk list
		 */
		virtual const std::vector<std::shared_ptr<Terrain>>& GetVisibleChunks() const = 0;

		/**
		 * @brief Get a thread-safe copy of visible chunks.
		 *
		 * Creates a snapshot of the visible chunks that can be safely
		 * accessed from other threads.
		 *
		 * @return Copy of visible chunk list
		 */
		virtual std::vector<std::shared_ptr<Terrain>> GetVisibleChunksCopy() const = 0;

		// ==================== Render Manager Integration ====================

		/**
		 * @brief Set the render manager for GPU-accelerated instanced rendering.
		 *
		 * When set, chunks are registered with the render manager instead of
		 * using per-chunk GPU resources. This enables single-draw-call
		 * rendering for all terrain.
		 *
		 * @param manager The render manager (nullptr to disable batched rendering)
		 */
		virtual void SetRenderManager(std::shared_ptr<TerrainRenderManager> manager) = 0;

		/**
		 * @brief Get the current render manager.
		 *
		 * @return Current render manager, or nullptr if not set
		 */
		virtual std::shared_ptr<TerrainRenderManager> GetRenderManager() const = 0;

		/**
		 * @brief Notify that a chunk was evicted from GPU memory.
		 *
		 * Called by the render manager when LRU eviction removes a chunk
		 * from the GPU texture array. Implementations may choose to
		 * invalidate CPU cache or simply re-register when visible again.
		 *
		 * @param chunk_key The (chunk_x, chunk_z) key of the evicted chunk
		 */
		virtual void InvalidateChunk(std::pair<int, int> chunk_key) = 0;

		// ==================== Terrain Queries ====================

		/**
		 * @brief Calculate terrain height and surface normal at a world position.
		 *
		 * Uses procedural generation to determine the terrain surface properties.
		 * Applies Phong tessellation interpolation for smooth results matching the GPU rendering.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @return Tuple of (height, surface_normal)
		 */
		virtual std::tuple<float, glm::vec3> CalculateTerrainPropertiesAtPoint(float x, float z) const = 0;

		/**
		 * @brief Get terrain properties at a point, preferring cached chunk data.
		 *
		 * Much faster than CalculateTerrainPropertiesAtPoint() when querying within visible terrain.
		 * Falls back to procedural generation for uncached areas.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @return Tuple of (height, surface_normal)
		 */
		virtual std::tuple<float, glm::vec3> GetTerrainPropertiesAtPoint(float x, float z) const = 0;

		/**
		 * @brief Check if a point is below the terrain surface.
		 *
		 * @param point 3D world position
		 * @return true if point.y < terrain height at (point.x, point.z)
		 */
		virtual bool IsPointBelowTerrain(const glm::vec3& point) const = 0;

		/**
		 * @brief Get signed vertical distance from terrain surface.
		 *
		 * @param point 3D world position
		 * @return Positive if above terrain, negative if below
		 */
		virtual float GetDistanceAboveTerrain(const glm::vec3& point) const = 0;

		/**
		 * @brief Check if a world position is within cached terrain.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @return true if position is in a cached chunk
		 */
		virtual bool IsPositionCached(float x, float z) const = 0;

		// ==================== Raycasting ====================

		/**
		 * @brief Cast a ray against the terrain surface.
		 *
		 * Uses ray marching with binary search refinement for accuracy.
		 *
		 * @param origin Ray start position
		 * @param direction Ray direction (should be normalized)
		 * @param max_distance Maximum ray distance
		 * @param out_distance Output: distance to hit point
		 * @return true if terrain was hit within max_distance
		 */
		virtual bool
		Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, float& out_distance) const = 0;

		/**
		 * @brief Cast a ray with normal output.
		 *
		 * @param origin Ray start position
		 * @param direction Ray direction (normalized)
		 * @param max_distance Maximum ray distance
		 * @param out_distance Output: distance to hit point
		 * @param out_normal Output: surface normal at hit point
		 * @return true if terrain was hit
		 */
		virtual bool RaycastCached(
			const glm::vec3& origin,
			const glm::vec3& direction,
			float            max_distance,
			float&           out_distance,
			glm::vec3&       out_normal
		) const = 0;

		// ==================== Terrain Deformation ====================

		/**
		 * @brief Get the deformation manager for modifying terrain.
		 *
		 * @return Reference to the deformation manager
		 */
		virtual TerrainDeformationManager&       GetDeformationManager() = 0;
		virtual const TerrainDeformationManager& GetDeformationManager() const = 0;

		/**
		 * @brief Add a crater deformation.
		 *
		 * @param center Crater center position
		 * @param radius Crater radius
		 * @param depth Crater depth (positive = deeper)
		 * @param irregularity Random variation 0-1
		 * @param rim_height Height of crater rim
		 * @return ID of created deformation
		 */
		virtual uint32_t AddCrater(
			const glm::vec3& center,
			float            radius,
			float            depth,
			float            irregularity = 0.2f,
			float            rim_height = 0.0f
		) = 0;

		/**
		 * @brief Add a flatten square deformation.
		 *
		 * @param center Center position (Y = target height)
		 * @param half_width Half-width in X direction
		 * @param half_depth Half-depth in Z direction
		 * @param blend_distance Edge blending distance
		 * @param rotation_y Rotation around Y axis (radians)
		 * @return ID of created deformation
		 */
		virtual uint32_t AddFlattenSquare(
			const glm::vec3& center,
			float            half_width,
			float            half_depth,
			float            blend_distance = 1.0f,
			float            rotation_y = 0.0f
		) = 0;

		/**
		 * @brief Add an Akira deformation (hemispherical removal).
		 *
		 * @param center Center position
		 * @param radius Radius of the cut at terrain level
		 * @return ID of created deformation
		 */
		virtual uint32_t AddAkira(const glm::vec3& center, float radius) = 0;

		/**
		 * @brief Invalidate and regenerate chunks affected by deformations.
		 *
		 * @param deformation_id Optional: only invalidate for specific deformation
		 */
		virtual void InvalidateDeformedChunks(std::optional<uint32_t> deformation_id = std::nullopt) = 0;

		// ==================== Terrain Properties ====================

		/**
		 * @brief Get the maximum terrain height.
		 *
		 * Used for frustum culling and visualization bounds.
		 *
		 * @return Maximum height value from biome configuration
		 */
		virtual float GetMaxHeight() const = 0;

		/**
		 * @brief Get chunk size in world units.
		 *
		 * @return Size of each terrain chunk (typically 32)
		 */
		virtual int GetChunkSize() const = 0;

		/**
		 * @brief Set the global scale of the world.
		 *
		 * Scaling affects both horizontal and vertical terrain features.
		 * Values > 1.0 make the world feel larger (more expanded features).
		 * Values < 1.0 make the world feel smaller (more compressed features).
		 *
		 * @param scale The new world scale (default: 1.0)
		 */
		virtual void SetWorldScale(float scale) = 0;

		/**
		 * @brief Get the current world scale.
		 *
		 * @return The current world scale
		 */
		virtual float GetWorldScale() const = 0;

		/**
		 * @brief Get a version counter that increments whenever the terrain changes.
		 *
		 * This counter increments when the world scale changes or when deformations
		 * are added/removed, signaling systems like DecorManager to regenerate.
		 *
		 * @return The current terrain version
		 */
		virtual uint32_t GetVersion() const = 0;

		/**
		 * @brief Get a path along terrain following procedural path spline.
		 *
		 * @param start_pos Starting XZ position
		 * @param num_points Number of path points to generate
		 * @param step_size Distance between points
		 * @return Vector of 3D path points on terrain surface
		 */
		virtual std::vector<glm::vec3> GetPath(glm::vec2 start_pos, int num_points, float step_size) const = 0;
		virtual glm::vec3              GetPathData(float x, float z) const = 0;
		virtual float                  GetBiomeControlValue(float x, float z) const = 0;

	protected:
		// Protected constructor - only derived classes can instantiate
		ITerrainGenerator() = default;

		// Non-copyable
		ITerrainGenerator(const ITerrainGenerator&) = delete;
		ITerrainGenerator& operator=(const ITerrainGenerator&) = delete;
	};

} // namespace Boidsish
